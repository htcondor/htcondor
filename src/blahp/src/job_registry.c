/*
 *  File :     job_registry.c
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  12-Nov-2007 Original release
 *  27-Feb-2008 Added user_prefix to classad printout.
 *              Added job_registry_split_blah_id and its free call.
 *  3-Mar-2008  Added non-privileged updates to fit CREAM's file and process
 *              ownership model.
 *  8-Jan-2009  Added job_registry_update_select call.
 * 20-Oct-2009  Added update_recn calls to save on record lookup operations.
 *              and job_registry_unlock call.
 * 23-Oct-2009  Avoid unnecessary registry sort when the disk registry is unchanged.
 *              Make sure 'firstrec' is recomputed for record sanity checks.
 * 19-Nov-2009  Added BY_USER_PREFIX as an indexing mode.
 *  4-Feb-2010  Added updater_info field to store updater state.
 *  20-Sep-2010 Added JOB_REGISTRY_UNCHANGED return code to update ops.
 *  21-Sep-2010 Added JOB_REGISTRY_BINFO_ONLY return code. Updater_info
 *              changes don't affect entry mdate.
 *   7-Oct-2010 Added support for optional mmap sharing 
 *              of entry index.
 *  11-Mar-2011 Removed ID uniqueness check from 
 *              job_registry_merge_pending_nonpriv_updates
 *              (for efficiency reasons: ID uniqueness in that
 *              case is guaranteed by the invoking service).
 *              Added job_registry_check_index_key_uniqueness.
 *  21-Jul-2011 Added job_registry_need_update function.
 *  11-Sep-2015 Always return most recent job in job_registry_get_recnum.
 *
 *  Description:
 *    File-based container to cache job IDs and statuses to implement
 *    bulk status commands in BLAH.
 *
 *  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
 *
 *    See http://www.eu-egee.org/partners/ for details on the copyright
 *    holders.  
 *  
 *    Licensed under the Apache License, Version 2.0 (the "License"); 
 *    you may not use this file except in compliance with the License. 
 *    You may obtain a copy of the License at 
 *  
 *        http://www.apache.org/licenses/LICENSE-2.0 
 *  
 *    Unless required by applicable law or agreed to in writing, software 
 *    distributed under the License is distributed on an "AS IS" BASIS, 
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 *    See the License for the specific language governing permissions and 
 *    limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>
#include <errno.h>
#undef NDEBUG
#include <assert.h>

#include "job_registry.h"

#include "md5.h"

/*
 * jobregistry_construct_path
 *
 * Assemble dirname and basename of 'path', and an optional numeric
 * argument into a dynamically-allocated string formatted according to
 * 'format'.
 * 
 * @param format Format string including two %s and an optional %d converter
 *               These will be filled with the dirname, basename and numeric
 *               argument (if > 0).
 * @param path   Full file path name
 * @param num    Optional numerical argument. Will be taken into account
 *               only if it is greater than 0.
 *
 * @return       Dynamically allocated string containing the formatting
 *               result. Needs to be free'd.
 *
 */

char *
jobregistry_construct_path(const char *format, const char *path, 
                           unsigned int num)
 {
  char *pcopy1=NULL, *pcopy2=NULL;
  char *basepath, *dirpath;
  char *retpath = NULL;
  unsigned int ntemp, numlen;

  /* Make copies of the path as basename() and dirname() will change them. */
  pcopy1 = strdup(path);
  if (pcopy1 == NULL)
   {
    errno = ENOMEM;
    return NULL;
   }
  pcopy2 = strdup(path);
  if (pcopy2 == NULL)
   {
    errno = ENOMEM;
    free(pcopy1);
    return NULL;
   }
  
  if (num > 0)
   {
    /* Count digits in num */
    ntemp = num; numlen = 1;
    while (ntemp > 0)
     {
      if ((int)(ntemp/=10) > 0) numlen++;
     }
   }
  else numlen = 0;

  basepath = basename(pcopy1);
  dirpath  = dirname(pcopy2);

  retpath = (char *)malloc(strlen(basepath) + strlen(dirpath) + 
                               strlen(format) + numlen); 

  if (retpath == NULL)
   {
    errno = ENOMEM;
    free(pcopy1);
    free(pcopy2);
    return NULL;
   }

  if (num > 0) sprintf(retpath,format,dirpath,basepath,num);
  else         sprintf(retpath,format,dirpath,basepath);
  free(pcopy1);
  free(pcopy2); 

  return retpath;
}

/*
 * job_registry_purge
 *
 * Remove from the registry located in 'path' all entries that were
 * created before 'oldest_creation_date'.
 * This call is meant to be issued before the registry is open and
 * before job_registry_init() is called, as it has the ability to recover 
 * corrupted files if possible. The file will not be scanned and rewritten
 * if the first entry is more recent than 'oldest_creation_date' unless
 * 'force_rewrite' is true. In the latter case the file will be rescanned
 * and rewritten.
 *
 * @param path Path to the job registry file.
 * @param oldest_creation_date Oldest cdate of entries that will be
 *        kept in the registry.
 * @param force_rewrite If this evaluates to true, it will cause the 
 *        registry file to be entirely scanned and rewritten also in case
 *        no purging is needed.
 *
 * @return Less than zero on error. errno is set in case of error.
 *
 */

int 
job_registry_purge(const char *path, time_t oldest_creation_date,
                   int force_rewrite)
{
  FILE *fd,*fdw;
  struct flock wlock;
  char *newreg_path=NULL;
  job_registry_entry first,cur;
  job_registry_recnum_t last_recnum;
  int ret;
  mode_t old_umask;
  job_registry_handle *jra;
  job_registry_hash_store hst;

  jra = job_registry_init(path, NAMES_ONLY);
  if (jra == NULL) return -1;

  fd = fopen(jra->path,"r+");
  if (fd == NULL) 
   {
    job_registry_destroy(jra);
    return JOB_REGISTRY_FOPEN_FAIL;
   }

  /* Now obtain the requested write lock */

  wlock.l_type = F_WRLCK;
  wlock.l_whence = SEEK_SET;
  wlock.l_start = 0;
  wlock.l_len = 0; /* Lock whole file */
  
  if (fcntl(fileno(fd), F_SETLKW, &wlock) < 0) 
   {
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_FLOCK_FAIL;
   }

  if ((ret = job_registry_seek_next(fd,&first)) < 0)
   {
    int result;	
    if (force_rewrite) {
        result = ftruncate(fileno(fd), 0);
        assert(result == 0);
    }
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_NO_VALID_RECORD;
   }
  if (ret == 0)
   {
    /* End of file. */
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_SUCCESS;
   }
  
  if ( (first.magic_start != JOB_REGISTRY_MAGIC_START) ||
       (first.magic_end   != JOB_REGISTRY_MAGIC_END) )
   {
    errno = ENOMSG;
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_CORRUPT_RECORD;
   }

  last_recnum = first.recnum;

  if (!force_rewrite && (first.cdate >= oldest_creation_date))
   {
    /* Nothing to purge. Go home. */
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_SUCCESS;
   }

  /* Create purged registry file that will be rotated in place */
  /* of the current registry. */
  newreg_path = jobregistry_construct_path("%s/%s.new.%d", jra->path, getpid());
  if (newreg_path == NULL) 
   {
    fclose(fd);
    job_registry_destroy(jra);
    return JOB_REGISTRY_MALLOC_FAIL;
   }
  
  /* Make sure the file is group writable. */
  old_umask=umask(S_IWOTH);
  fdw = fopen(newreg_path,"w");
  umask(old_umask);
  if (fdw == NULL)
   {
    fclose(fd);
    free(newreg_path);
    job_registry_destroy(jra);
    return JOB_REGISTRY_FOPEN_FAIL;
   }

  if (first.cdate >= oldest_creation_date)
   {
    /* force_rewrite must be on. Write out the first record if needed */
    if (fwrite(&first,sizeof(job_registry_entry),1,fdw) < 1)
     {
      fclose(fd);
      fclose(fdw);
      free(newreg_path);
      job_registry_destroy(jra);
      return JOB_REGISTRY_FWRITE_FAIL;
     }
   }
  else job_registry_unlink_proxy(jra, &first);

  hst.data = NULL;
  hst.n_data = 0;

  while (!feof(fd))
   {
    if ((ret = job_registry_seek_next(fd,&cur)) < 0)
     {
      fclose(fd);
      fclose(fdw);
      free(newreg_path);
      job_registry_destroy(jra);
      job_registry_free_hash_store(&hst); 
      return JOB_REGISTRY_NO_VALID_RECORD;
     }
    if (ret == 0) break;

    if (cur.cdate < oldest_creation_date)
     {
      job_registry_unlink_proxy(jra, &cur);
      continue;
     }
    /* Sanitize sequence numbers */
    if (cur.recnum != (last_recnum+1)) cur.recnum = last_recnum + 1;
    last_recnum = cur.recnum;

    job_registry_store_hash(&hst, cur.subject_hash);

    if (fwrite(&cur,sizeof(job_registry_entry),1,fdw) < 1)
     {
      fclose(fd);
      fclose(fdw);
      free(newreg_path);
      job_registry_destroy(jra);
      job_registry_free_hash_store(&hst); 
      return JOB_REGISTRY_FWRITE_FAIL;
     }
   }

  fclose(fdw);

  if (rename(newreg_path, jra->path) < 0)
   {
    fclose(fd);
    free(newreg_path);
    job_registry_destroy(jra);
    job_registry_free_hash_store(&hst); 
    return JOB_REGISTRY_RENAME_FAIL;
   }

  /* Closing fd will release the write lock. */
  fclose (fd);

  job_registry_purge_subject_hash_list(jra, &hst);
  job_registry_free_hash_store(&hst); 

  free(newreg_path);

  job_registry_destroy(jra);

  return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_probe_next_record
 *
 * Guess the record size in the registry file by looking for record magic
 * numbers and matching them to a set of known sizes.
 * Read a record that fits into our current job_registry_entry structure.
 *
 * @param fd File descriptor of an open and read-locked registry file.
 * @param entry to be filled with data.
 *
 * @return Zero in case of error. Record size as found on disk on success.
 *
 */

size_t
job_registry_probe_next_record(FILE *fd, job_registry_entry *en)
{
  size_t rsize = 0;
  size_t act_size;
  long start_pos, end_pos = 0;
  int sret, eret, cret;
  int ic;
  size_t allowed_size_incs[N_JOB_REGISTRY_ALLOWED_ENTRY_SIZE_INCS] =
                             JOB_REGISTRY_ALLOWED_ENTRY_SIZE_INCS;

  while (!feof(fd))
   {
    start_pos = ftell(fd);
    if (start_pos < 0) return start_pos;

    if ((sret=fread(&en->magic_start, 1, sizeof(en->magic_start), fd)) < 
        (int)sizeof(en->magic_start)) break;
    if (en->magic_start != JOB_REGISTRY_MAGIC_START)
     {
      fseek(fd, -sret+1, SEEK_CUR);
      continue;
     }

    for(ic=0;allowed_size_incs[ic] != 0;ic++)
     {
      fseek(fd, allowed_size_incs[ic], SEEK_CUR);

      if ((eret=fread(&en->magic_end, 1, sizeof(en->magic_end), fd)) < (int)sizeof(en->magic_end)) break;
      end_pos = ftell(fd);
      if (end_pos < 0) return end_pos;

      if (en->magic_end == JOB_REGISTRY_MAGIC_END)
       {
        /* Declare an end-of-record only if at end-of-file or if */
        /* followed by a start-of-record */
        if ((cret=fread(&en->magic_start, 1, sizeof(en->magic_start), fd)) == sizeof(en->magic_start))
         {
          if (en->magic_start == JOB_REGISTRY_MAGIC_START)
           {
            fseek(fd, end_pos, SEEK_SET);
            break;
           }
         }
        else
         {
			 /* I have no idea what the following was supposed to do
			    It doesn't do anything, so we'll comment it out for now.
          if (feof(fd)) en->magic_start == JOB_REGISTRY_MAGIC_START;
		  */
          break;
         }
       }
      fseek(fd, end_pos-eret, SEEK_SET);
     }

    if (en->magic_start == JOB_REGISTRY_MAGIC_START &&
        en->magic_end == JOB_REGISTRY_MAGIC_END)
     {
      rsize = end_pos - start_pos;
      break;
     }
   }

  if (rsize > 0)
   {
    if (fseek(fd, start_pos, SEEK_SET) < 0) return -1;
    if (rsize > sizeof(*en)) act_size = sizeof(*en);
    else act_size = rsize;

    memset(en, 0, sizeof(*en));

    if (fread(en, act_size - sizeof(en->magic_end), 1, fd) < 1) return -1;
    if (fseek(fd, end_pos, SEEK_SET) < 0) return -1;

    en->magic_end = JOB_REGISTRY_MAGIC_END;
   }

  return rsize;
}

/*
 * job_registry_update_reg
 *
 * Converts an old registry to a newer format at path rha->path. This assumes
 * The registry entry structure is always expanded at the tail,
 * before the final magic number, and that zeroing out the new
 * part is OK.
 *
 * @param rha Pointer to a job registry handle. The current lockfile
 *            will be obtained.
 * @param old_path Pathname of the old job registry file.
 *
 */

int
job_registry_update_reg(const job_registry_handle *rha,
                        const char *old_path)
{
  FILE *of, *nf;
  job_registry_entry en;
  int encount;
  int rret = 0;
  int wret = 0;

  of = fopen(old_path,"r");
  if (of == NULL) return -1;

  nf = fopen(rha->path,"w");
  if (nf == NULL)
   {
    fclose(of);
    return -1;
   }
  if (job_registry_wrlock(rha,nf) < 0)
   {
    fclose(of);
    fclose(nf);
    return -1;
   }

  encount = 0;
  while (!feof(of))
   {
    if ((rret = job_registry_probe_next_record(of, &en)) == 0) break;
    if ((wret = fwrite(&en, sizeof(en), 1, nf)) < 1) break;
    else encount++;
   }

  fclose(of);
  fclose(nf);

  if (wret < 1 || rret < 0)
   {
    unlink(rha->path);
    return -1;
   }
  else unlink(old_path);

  return encount;
}

/*
 * job_registry_init
 *
 * Sets up for access to a job registry file pointed to by 'path'.
 * An entry index is loaded in memory if mode is not set to NO_INDEX.
 *
 * @param path Pathname of the job registry file.
 * @param mode Operation of the index cache: NO_INDEX disables the cache
 *             (job_registry_lookup, job_registry_update and job_registry_get
 *             cannot be used), BY_BATCH_ID uses batch_id as the cache index 
 *             key,  BY_BLAH_ID sets blah_id as the cache index key and
 *             BY_USER_PREFIX uses the user_prefix entry (must be unique!).
 *             NAMES_ONLY will fill the rha structure with the appropriate
 *             file paths without performing any file access.
 *
 * @return Pointer to a dynamically allocated handle to the job registry.
 *         This needs to be freed via job_registry_destroy.
 */

job_registry_handle *
job_registry_init(const char *path,
                  job_registry_index_mode mode)
{
  job_registry_handle *rha;
  job_registry_entry junk;
  size_t found_size;
  struct stat fst, lst, dst;
  char real_file_name[FILENAME_MAX];
  int rlnk_status;
  mode_t old_umask;
  int cfd;
  FILE *fd;
  char *old_lockfile, *old_npudir, *old_path=NULL;
  int need_to_create_regdir = FALSE;
  char *dircreat[3];
  char **dir;
  int stret;

  rha = (job_registry_handle *)calloc(1, sizeof(job_registry_handle));
  if (rha == NULL) 
   {
    errno = ENOMEM;
    return NULL;
   }

  rha->mode = mode;
  rha->disk_firstrec = 0;

  /* Resolve symbolic link, if any */
  if (lstat(path, &fst) >= 0)
   {
    if (S_ISLNK(fst.st_mode))
     {
      rlnk_status = readlink(path, real_file_name, sizeof(real_file_name));
      if (rlnk_status > 0 && (size_t)rlnk_status < sizeof(real_file_name))
       {
        real_file_name[rlnk_status] = '\000'; /* readlink does not NULL-terminate */
        path = real_file_name;
       }
     }
   }

  rha->path = (char *)malloc(strlen(path) + strlen(JOB_REGISTRY_REGISTRY_NAME) + 2);
  if (rha->path == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }
  sprintf(rha->path, "%s/%s", path, JOB_REGISTRY_REGISTRY_NAME);

  /* Create path and dir for NPU storage */
  rha->npudir = jobregistry_construct_path("%s/%s.npudir",rha->path,0);
  if (rha->npudir == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }

  /* Check for reg dir existance, and convert from simple-file format */
  /* in a backwards-compatible fashion */
  if (mode != NAMES_ONLY)
   {
    stret = lstat(path, &fst);
    if (stret < 0 && errno == ENOENT) need_to_create_regdir = TRUE;
    if (stret >= 0)
     {
      if (!S_ISDIR(fst.st_mode))
       {
        old_lockfile = jobregistry_construct_path("%s/.%s.locktest",path,0);
        old_npudir = jobregistry_construct_path("%s/.%s.npudir",path,0);
        if (old_lockfile == NULL || old_npudir == NULL)
         {
          if (old_lockfile != NULL) free(old_lockfile);
          if (old_npudir != NULL) free(old_npudir);
          job_registry_destroy(rha);
          errno = ENOMEM;
          return NULL;
         }
        old_path = jobregistry_construct_path("%s/%s-OLD",path,0);
        if (old_path == NULL)
         {
          free(old_lockfile); free(old_npudir);
          job_registry_destroy(rha);
          errno = ENOMEM;
          return NULL;
         }
        if (rename(path, old_path) < 0)
         {
          free(old_lockfile); free(old_npudir); free(old_path);
          job_registry_destroy(rha);
          return NULL; /* keep the rename errno */
         }
        need_to_create_regdir = TRUE;
       }
      else
       {
        /* Make sure the file has as-restrictive as possible permissions */
        chmod(path, fst.st_mode&(~(S_IROTH|S_IWOTH)));
       }
     }
    if (need_to_create_regdir)
     {
      old_umask = umask(0);
      if (mkdir(path,S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IXOTH) < 0)
       {
        if (old_path != NULL) 
         {
          free(old_lockfile); free(old_npudir); free(old_path);
         }
        umask(old_umask);
        job_registry_destroy(rha);
        return NULL;
       }
      umask(old_umask);
      if (old_path != NULL)
       {
        rename(old_npudir, rha->npudir); /* An error here would deserve a
                                            warning, but there's no
                                            way to feed it back */
        free(old_npudir);
        unlink(old_lockfile); /* No real worry if this fails */
        free(old_lockfile);
        if (job_registry_update_reg(rha, old_path) < 0)
         {
          free(old_path);
          job_registry_destroy(rha);
          return NULL; /* keep the rename errno */
         }
       }
	  if (old_path) free(old_path);
     }
   }

  /* Create path for lock test file */
  rha->lockfile = jobregistry_construct_path("%s/%s.locktest",rha->path,0);
  if (rha->lockfile == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }

  /* Create path for subject list */
  rha->subjectlist = jobregistry_construct_path("%s/%s.subjectlist",rha->path,0);
  if (rha->subjectlist == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }
  /* Create path for non-privileged updates to the subject list */
  rha->npusubjectlist = jobregistry_construct_path("%s/%s.npusubjectlist",rha->path,0);
  if (rha->npusubjectlist == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }
  /* Create path for mmap-pable shared entry index */
  rha->index_mmap_length = 0;
  rha->mmap_fd = -1;
  rha->mmappableindex = NULL;
  if (mode == BY_BLAH_ID_MMAP || mode == BY_BATCH_ID_MMAP ||
      mode == BY_USER_PREFIX_MMAP)
   {
    switch (mode)
     {
      case BY_BLAH_ID_MMAP:
        rha->mmappableindex = jobregistry_construct_path("%s/%s.by_blah_index",rha->path,0);
        break;
      case BY_BATCH_ID_MMAP:
        rha->mmappableindex = jobregistry_construct_path("%s/%s.by_batch_index",rha->path,0);
        break;
      case BY_USER_PREFIX_MMAP:
        rha->mmappableindex = jobregistry_construct_path("%s/%s.by_user_index",rha->path,0);
        break;
      default:
        break;
     }
    if (rha->mmappableindex == NULL)
     {
      job_registry_destroy(rha);
      errno = ENOMEM;
      return NULL;
     }
   }

  if (mode != NAMES_ONLY)
   {
    if (stat(rha->lockfile, &lst) < 0)
     {
      if (errno == ENOENT)
       {
        old_umask = umask(0);
        if ((cfd=creat(rha->lockfile,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
         {
          umask(old_umask);
          job_registry_destroy(rha);
          return NULL;
         }
	close(cfd);
        umask(old_umask);
       }
      else
       {
        job_registry_destroy(rha);
        return NULL;
       }
     }
    else
     {
      /* Make sure the file is empty has as-restrictive as possible permissions */
      chmod(rha->lockfile, lst.st_mode&(~(S_IXUSR|S_IXGRP|S_IXOTH)));
      int result;	
      result = truncate(rha->lockfile, 0);
      assert(result == 0);
     }
    if (stat(rha->subjectlist, &lst) < 0)
     {
      if (errno == ENOENT)
       {
        old_umask = umask(0);
        if ((cfd=creat(rha->subjectlist,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) < 0)
         {
          umask(old_umask);
          job_registry_destroy(rha);
          return NULL;
         }
	close(cfd);
        umask(old_umask);
       }
      else
       {
        job_registry_destroy(rha);
        return NULL;
       }
     }
    else
     {
      /* Make sure the file has as-restrictive as possible permissions */
      chmod(rha->subjectlist, lst.st_mode&(~(S_IXUSR|S_IXGRP|S_IXOTH|S_IWOTH)));
     }
   }

  /* Create path and dir for proxy storage */
  rha->proxydir = jobregistry_construct_path("%s/%s.proxydir",rha->path,0);
  if (rha->proxydir == NULL)
   {
    job_registry_destroy(rha);
    errno = ENOMEM;
    return NULL;
   }


  if (mode != NAMES_ONLY)
   {
    dircreat[0] = rha->npudir;
    dircreat[1] = rha->proxydir;
    dircreat[2] = NULL;

    for(dir = dircreat; *dir != NULL; dir++)
     {
      if (stat(*dir, &dst) < 0)
       {
        if (errno == ENOENT)
         {
          old_umask = umask(0);
          if (mkdir(*dir,S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH) < 0)
           {
            umask(old_umask);
            job_registry_destroy(rha);
            return NULL;
           }
          umask(old_umask);
         }
        else
         {
          job_registry_destroy(rha);
          return NULL;
         }
       }
      else
       {
        if (!S_ISDIR(dst.st_mode))
         {
          job_registry_destroy(rha);
          errno = ENOTDIR;
          return NULL;
         }
        /* Make sure the file has as-restrictive as possible permissions */
        chmod(*dir, dst.st_mode&(~(S_IROTH)));
       }
     }
   }
  
  if (mode == NAMES_ONLY) return(rha);

  /* Now read the entire file and cache its contents */

  /* In NO_INDEX mode only firstrec and lastrec will be updated */
  /* Don't merge pending updates (use fopen instead of job_registry_open), as */
  /* we may be in need of a registry format update. */

  fd = fopen(rha->path, "r");
  if (fd == NULL)
   {
    /* Make sure the file is group writable. */
    old_umask = umask(S_IWOTH);
    if (errno == ENOENT) fd = fopen(rha->path, "w+");
    umask(old_umask);
    if (fd == NULL)
     {
      job_registry_destroy(rha);
      return NULL;
     }
   }
  if (job_registry_rdlock(rha, fd) < 0)
   {
    fclose(fd);
    job_registry_destroy(rha);
    return NULL;
   }

  /* Is the record structure on disk of a different size than our current one ? */
  /* We may need a registry update. */

  found_size = job_registry_probe_next_record(fd, &junk);

  if ((found_size > 0) && (found_size != sizeof(job_registry_entry)))
   {
    /* Move old registry away and update it */
    old_path = jobregistry_construct_path("%s/%s.oldreg",rha->path,0);
    if (old_path == NULL)
     {
      job_registry_destroy(rha);
      errno = ENOMEM;
      fclose(fd);
      return NULL;
     }
    if (rename(rha->path, old_path) < 0)
     {
      free(old_path);
      job_registry_destroy(rha);
      fclose(fd);
      return NULL;
     }
    fclose(fd); /* Release lock */
    if (job_registry_update_reg(rha, old_path) < 0)
     {
      free(old_path);
      job_registry_destroy(rha);
      return NULL; /* keep the rename errno */
     }
    free(old_path);
   }
  else fclose(fd);

  /* Now it's safe to attempt a NPU merge */
  job_registry_merge_pending_nonpriv_updates(rha, NULL);

  fd = fopen(rha->path, "r");
  if (fd == NULL)
   {
    job_registry_destroy(rha);
     return NULL;
   }
  if (job_registry_rdlock(rha, fd) < 0)
   {
    fclose(fd);
    job_registry_destroy(rha);
    return NULL;
   }

  if (job_registry_resync(rha, fd) < 0)
   {
    fclose(fd);
    job_registry_destroy(rha);
    return NULL;
   }
     
  fclose(fd);

  return(rha);
}

/*
 * job_registry_destroy
 *
 * Frees any dynamic content and the job registry handle itself.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init
 */

void 
job_registry_destroy(job_registry_handle *rha)
{
   if (rha->path != NULL) free(rha->path);
   if (rha->lockfile != NULL) free(rha->lockfile);
   if (rha->npudir != NULL) free(rha->npudir);
   if (rha->proxydir != NULL) free(rha->proxydir);
   if (rha->subjectlist != NULL) free(rha->subjectlist);
   if (rha->npusubjectlist != NULL) free(rha->npusubjectlist);
   if (rha->mmappableindex != NULL) free(rha->mmappableindex);
   rha->n_entries = rha->n_alloc = 0;
   if (rha->index_mmap_length > 0)
    {
     munmap(rha->entries, rha->index_mmap_length);
     close(rha->mmap_fd);
     rha->index_mmap_length = 0;
     rha->mmap_fd = -1;
    }
   else if (rha->entries != NULL) free(rha->entries);
   free(rha);
}

/*
 * job_registry_firstrec
 * 
 * Get the record number of the first record in the open registry file
 * pointed to by fd. 
 * Record numbers are supposed to be in ascending order, with no gaps. 
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init
 * @param fd Stream descriptor of an open registry file.
 *        fd *must* be at least read locked before calling this function. 
 *        The file will be left positioned after the first record.
 *
 * @return Record number in the first record of the file.
 *
 */

job_registry_recnum_t
job_registry_firstrec(job_registry_handle *rha, FILE *fd)
{
  int ret;
  job_registry_entry first; 

  rha->disk_firstrec = 0;

  ret = fseek(fd, 0L, SEEK_SET);
  if (ret < 0) return ret; 

  ret = fread(&first, sizeof(job_registry_entry), 1, fd);
  if (ret < 1) 
   {
    if (feof(fd)) return 0;
    else return JOB_REGISTRY_FREAD_FAIL;
   }

  if ( (first.magic_start != JOB_REGISTRY_MAGIC_START) ||
       (first.magic_end   != JOB_REGISTRY_MAGIC_END) )
   {
    errno = ENOMSG;
    return JOB_REGISTRY_CORRUPT_RECORD;
   }

  rha->disk_firstrec = first.recnum;
  return first.recnum;
}

/*
 * job_registry_resync_mmap
 *
 * Update an index cache on file that can be shared among different
 * processes.
 * 
 * @param fd Stream descriptor of an open registry file.
 *        fd *must* be at least read locked before calling this function. 
 * @param rha Pointer to a job registry handle returned by job_registry_init
 *
 * @return Less than zero on error. Zero on unchanged registry. Greater than 0
 *         if registry changed, See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_resync_mmap(job_registry_handle *rha, FILE *fd)
{
  struct stat rst, mst;
  job_registry_recnum_t firstrec;
  job_registry_recnum_t old_firstrec, old_lastrec;
  char *chosen_id;
  int mst_ret;
  job_registry_entry *ren;
  job_registry_index newin; 
  int need_to_update = TRUE;
  int update_exponential_backoff = 1;
  char *new_index;
  int newindex_fd=-1;
  int i;

  if (fstat(fileno(fd), &rst) < 0) return JOB_REGISTRY_STAT_FAIL;
  mst_ret = stat(rha->mmappableindex, &mst);
  if (mst_ret >= 0)
   {
    if ((mst.st_size/sizeof(job_registry_index)) ==
        (rst.st_size/sizeof(job_registry_entry))) need_to_update = FALSE;
   }

  firstrec = job_registry_firstrec(rha,fd);
  if (fseek(fd,0L,SEEK_SET) < 0) return JOB_REGISTRY_FSEEK_FAIL;

  if ((rha->lastrec != 0 || rha->firstrec != 0) && firstrec != rha->firstrec)
   {
    /* The registry may have been purged. */
    need_to_update = TRUE;
   }

  old_lastrec = rha->lastrec;
  old_firstrec = rha->firstrec;

  while (need_to_update)
   {
    new_index = (char *)malloc(strlen(rha->mmappableindex)+5);
    if (new_index == NULL) 
     {
      errno = ENOMEM;
      return JOB_REGISTRY_MALLOC_FAIL;
     }
    sprintf(new_index,"%s.tmp",rha->mmappableindex);

    newindex_fd = open(new_index, O_RDWR|O_CREAT|O_EXCL, 
                       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (newindex_fd < 0 && errno != EEXIST && errno != EACCES)
     {
      free(new_index);
      return JOB_REGISTRY_FOPEN_FAIL;
     }
    else if (newindex_fd < 0 && errno == EACCES)
     {
      /* If we cannot update the mmap file we are non-privileged and
         we'd better use the one that's there. */
      free(new_index);
      break;
     }
    else if (newindex_fd >= 0)
     {
      /* Dispose of old index. */
      if (rha->index_mmap_length > 0)
       {
        munmap(rha->entries, rha->index_mmap_length);
        close(rha->mmap_fd);
        rha->index_mmap_length = 0;
        rha->mmap_fd = -1;
       }
      else if (rha->entries != NULL) free(rha->entries);
      rha->entries = NULL;
      rha->firstrec = 0;
      rha->lastrec = 0;
      rha->n_entries = 0;
      rha->n_alloc = 0;

      while ((ren = job_registry_get_next(rha, fd)) != NULL)
       {
        if (rha->firstrec == 0) rha->firstrec = ren->recnum;
        if (ren->recnum > rha->lastrec) rha->lastrec = ren->recnum;
        (rha->n_entries)++;
        if (rha->mode == BY_BLAH_ID_MMAP)         chosen_id = ren->blah_id;
        else if(rha->mode == BY_USER_PREFIX_MMAP) chosen_id = ren->user_prefix;
        else                                      chosen_id = ren->batch_id;
      
        JOB_REGISTRY_ASSIGN_ENTRY(newin.id,chosen_id);
        newin.recnum = ren->recnum; 
        if (write(newindex_fd, &newin, sizeof(newin)) < (int)sizeof(newin))
         {
          free(ren);
          unlink(new_index);
          close(newindex_fd);
          free(new_index);
          rha->firstrec = 0;
          rha->lastrec = 0;
          rha->n_entries = 0;
          return JOB_REGISTRY_FWRITE_FAIL;
         }
        free(ren);
       } /* End of mmap file update cycle */

      if (rha->n_entries == 0)
       {
        /* Registry empty - no need to mmap */
        unlink(new_index);
        close(newindex_fd);
        free(new_index);
        return JOB_REGISTRY_NO_VALID_RECORD;
       }

      rha->index_mmap_length = lseek(newindex_fd, 0, SEEK_CUR);

      rha->entries = mmap(0, rha->index_mmap_length, 
                          PROT_READ|PROT_WRITE, MAP_SHARED, newindex_fd, 0);
      if (rha->entries == MAP_FAILED)
       {
        unlink(new_index);
        close(newindex_fd);
        free(new_index);
        rha->firstrec = 0;
        rha->lastrec = 0;
        rha->n_entries = 0;
        rha->entries = NULL;
        rha->index_mmap_length = 0;
        return JOB_REGISTRY_MMAP_FAIL;
       }
      job_registry_sort(rha);
      if (munmap(rha->entries, rha->index_mmap_length) < 0)
       {
        /* Something went wrong while syncing the sorted index to disk */
        unlink(new_index);
        close(newindex_fd);
        free(new_index);
        rha->firstrec = 0;
        rha->lastrec = 0;
        rha->n_entries = 0;
        rha->index_mmap_length = 0;
        return JOB_REGISTRY_MUNMAP_FAIL;
       }
      if (rename (new_index, rha->mmappableindex) < 0)
       {
        unlink(new_index);
        close(newindex_fd);
        free(new_index);
        rha->firstrec = 0;
        rha->lastrec = 0;
        rha->n_entries = 0;
        rha->index_mmap_length = 0;
        return JOB_REGISTRY_RENAME_FAIL;
       }
      rha->entries = mmap(0, rha->index_mmap_length, PROT_READ, MAP_SHARED, newindex_fd, 0);
      if (rha->entries == NULL)
       {
        close(newindex_fd);
        free(new_index);
        rha->firstrec = 0;
        rha->lastrec = 0;
        rha->n_entries = 0;
        rha->index_mmap_length = 0;
        return JOB_REGISTRY_MMAP_FAIL;
       }
      free(new_index);
      break; /* Done with registry update. */
     }

    /* If we reach here, the creation of the new mmappable index failed */
    /* with EEXIST, i.e. someone else is trying to update it. */
    /* We back off until (hopefully) this is complete. */

    sleep(update_exponential_backoff);
    update_exponential_backoff*=2;

    /* Did a good index (updated by somebody else) appear in the meanwhile ? */
    if (fstat(fileno(fd), &rst) < 0) {
		free(new_index);
		return JOB_REGISTRY_STAT_FAIL;
	}
    mst_ret = stat(rha->mmappableindex, &mst);
    if (mst_ret >= 0)
     {
      if ((mst.st_size/sizeof(job_registry_index)) ==
          (rst.st_size/sizeof(job_registry_entry)))
       { 
        free(new_index);
        break;
       }
     }

    if (update_exponential_backoff > JOB_REGISTRY_MMAP_UPDATE_TIMEOUT)
     {
      /* Unlinking the 'new' file after this long cannot hurt anyone */
      /* If there is a process that was really trying to write it, it */
      /* will just fall back to the in-memory index. */
      /* This will handle the case of a 'new_index' file left behind */
      /* by a dead process */
      if (unlink(new_index) < 0)
       {
        free(new_index);
        return JOB_REGISTRY_UPDATE_TIMEOUT;
       }
      /* We try creating the updated index again if we were able to unlink */
      /* the stale one. */
     }
    free(new_index);
   } /* End of while(need_to_update) loop */

  if (newindex_fd < 0)
   {
     off_t r;
    /* Need to attach to an existing and up-to-date mmap file */
    newindex_fd = open(rha->mmappableindex, O_RDONLY);
    if (newindex_fd < 0) return JOB_REGISTRY_FOPEN_FAIL;

    /* Dispose of old index. */
    if (rha->index_mmap_length > 0)
     {
      munmap(rha->entries, rha->index_mmap_length);
      close(rha->mmap_fd);
      rha->index_mmap_length = 0;
      rha->mmap_fd = -1;
     }
    else if (rha->entries != NULL) free(rha->entries);
    rha->entries = NULL;
    rha->firstrec = 0;
    rha->lastrec = 0;
    rha->n_entries = 0;
    rha->n_alloc = 0;

    r = lseek(newindex_fd, 0, SEEK_END);
    if (r < 0)
     {
      close(newindex_fd);
      rha->index_mmap_length = 0;
      return JOB_REGISTRY_FSEEK_FAIL;
     }
	rha->index_mmap_length = r;

    rha->entries = mmap(0, rha->index_mmap_length, PROT_READ, MAP_SHARED, newindex_fd, 0);
    if (rha->entries == NULL)
     {
      close(newindex_fd);
      rha->index_mmap_length = 0;
      return JOB_REGISTRY_MMAP_FAIL;
     }

    rha->n_entries = (rha->index_mmap_length)/sizeof(job_registry_index);
    rha->firstrec = JOB_REGISTRY_MAX_RECNUM;
    /* Compute first/last recnum from sorted index. */
    for (i=0; i<rha->n_entries; i++)
     {
      if (rha->entries[i].recnum < rha->firstrec) 
        rha->firstrec = rha->entries[i].recnum;
      if (rha->entries[i].recnum > rha->lastrec) 
        rha->lastrec = rha->entries[i].recnum;
     }
   }

  rha->mmap_fd = newindex_fd;

  if ((rha->lastrec != old_lastrec) || (rha->firstrec != old_firstrec))
   {
    return JOB_REGISTRY_CHANGED;
   }
  return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_resync
 *
 * Update the cache inside the job registry handle, if enabled,
 * otherwise just update rha->firstrec and rha->lastrec.
 * Will rescan the entire file in case this was found to be purged.
 * 
 * @param fd Stream descriptor of an open registry file.
 *        fd *must* be at least read locked before calling this function. 
 * @param rha Pointer to a job registry handle returned by job_registry_init
 *
 * @return Less than zero on error. Zero un unchanged registru. Greater than 0
 *         if registry changed, See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_resync(job_registry_handle *rha, FILE *fd)
{

  job_registry_recnum_t firstrec;
  job_registry_recnum_t old_firstrec, old_lastrec;
  job_registry_entry *ren;
  job_registry_index *new_entries;
  char *chosen_id;
  int mret;

  if (rha->mode == BY_BLAH_ID_MMAP || rha->mode == BY_BATCH_ID_MMAP ||
      rha->mode == BY_USER_PREFIX_MMAP)
   {
     if ((mret = job_registry_resync_mmap(rha, fd)) >= 0)
       return mret;
   }

  old_lastrec = rha->lastrec;
  old_firstrec = rha->firstrec;

  firstrec = job_registry_firstrec(rha,fd);

       /* File new or changed? Or mmap failed ? */
  if ( (rha->lastrec == 0 && rha->firstrec == 0) || firstrec != rha->firstrec
                                                 || rha->index_mmap_length > 0 )
   {
    if (fseek(fd,0L,SEEK_SET) < 0) return JOB_REGISTRY_FSEEK_FAIL;
    if (rha->index_mmap_length > 0)
     {
      munmap(rha->entries, rha->index_mmap_length);
      close(rha->mmap_fd);
      rha->index_mmap_length = 0;
      rha->mmap_fd = -1;
     }
    else if (rha->entries != NULL) free(rha->entries);
    rha->entries = NULL;
    rha->firstrec = 0;
    rha->lastrec = 0;
    rha->n_entries = 0;
    rha->n_alloc = 0;
   }
  else
   {
    /* Move to the last known end of file and keep on reading */
    if (fseek(fd,(long)((rha->lastrec - rha->firstrec + 1)*sizeof(job_registry_entry)),
              SEEK_SET) < 0) return JOB_REGISTRY_FSEEK_FAIL;
   }

  if (rha->mode == NO_INDEX) 
   {
    /* Just figure out the first and last recnum */
    if ((ren = job_registry_get_next(rha, fd)) != NULL)
     {
      if (rha->firstrec == 0) rha->firstrec = ren->recnum;
      free(ren);
      if (fseek(fd,(long)-sizeof(job_registry_entry),SEEK_END) >= 0)
       {
        if ((ren = job_registry_get_next(rha, fd)) != NULL)
         {
          rha->lastrec = ren->recnum;
          free(ren);
         }
        else return JOB_REGISTRY_NO_VALID_RECORD;
       }
     }
    return JOB_REGISTRY_SUCCESS;
   }

  while ((ren = job_registry_get_next(rha, fd)) != NULL)
   {
    if (rha->firstrec == 0) rha->firstrec = ren->recnum;
    if (ren->recnum > rha->lastrec) rha->lastrec = ren->recnum;
    (rha->n_entries)++;
    if (rha->n_entries > rha->n_alloc)
     {
      rha->n_alloc += JOB_REGISTRY_ALLOC_CHUNK;
      new_entries = realloc(rha->entries, 
                            rha->n_alloc * sizeof(job_registry_index));
      if (new_entries == NULL)
       {
        errno = ENOMEM;
        free(ren);
        return JOB_REGISTRY_MALLOC_FAIL;
       }
      rha->entries = new_entries;
     }
    if (rha->mode == BY_BLAH_ID)         chosen_id = ren->blah_id;
    else if(rha->mode == BY_USER_PREFIX) chosen_id = ren->user_prefix;
    else                                 chosen_id = ren->batch_id;

    JOB_REGISTRY_ASSIGN_ENTRY((rha->entries[rha->n_entries-1]).id,
                              chosen_id);
            
    (rha->entries[rha->n_entries-1]).recnum = ren->recnum; 
    free(ren);
   }
  if ((rha->lastrec != old_lastrec) || (rha->firstrec != old_firstrec))
   {
    job_registry_sort(rha);
    return JOB_REGISTRY_CHANGED;
   }
  return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_sort
 *
 * Will perform a non-recursive quicksort of the registry index.
 * This needs to be called before job_registry_lookup, as the latter
 * assumes the index to be ordered.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init
 *
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_sort(job_registry_handle *rha)
{
  /* Non-recursive quicksort of registry data */

  job_registry_sort_state *sst;
  job_registry_index swap;
  int i,k,kp,left,right=0,size,median;
  char median_id[JOBID_MAX_LEN];
  int n_sorted;

  /* Anything to do ? */
  if (rha->n_entries <= 1) return JOB_REGISTRY_SUCCESS;

  srand(time(0) & 0xfffffff);

  sst = (job_registry_sort_state *)malloc(rha->n_entries * 
              sizeof(job_registry_sort_state));
  if (sst == NULL)
   {
    errno = ENOMEM;
    return JOB_REGISTRY_MALLOC_FAIL;
   }
  
  sst[0] = LEFT_BOUND;
  for (i=1;i<(rha->n_entries - 1);i++) sst[i] = UNSORTED;
  sst[rha->n_entries - 1] = RIGHT_BOUND;

  for (n_sorted = 0; n_sorted < rha->n_entries; )
   {
    for (left=0; left<rha->n_entries; left=right+1)
     {
      if (sst[left] == SORTED) 
       {
        right = left;
        continue;
       }
      else if (sst[left] == LEFT_BOUND)
       {
        /* Find end of current sort partition */
        for (right = left+1; right<rha->n_entries; right++)
         {
          if (sst[right] == RIGHT_BOUND) break;
          else sst[right] = UNSORTED;
         }
        /* Separate entries from 'right' to 'left' and separate them at */
        /* entry 'median'. */
        size = (right - left + 1);
        median = rand()%size + left;
        JOB_REGISTRY_ASSIGN_ENTRY(median_id, rha->entries[median].id);
        for (i = left, k = right; ; i++,k--)
         {
          while (strcmp(rha->entries[i].id,median_id) < 0) i++;
          while (strcmp(rha->entries[k].id,median_id) > 0) k--;
          if (i>=k) break; /* Indices crossed ? */

          /* If we reach here entries 'i' and 'k' need to be swapped. */
          swap = rha->entries[i];
          rha->entries[i] = rha->entries[k];
          rha->entries[k] = swap;
         }
        /* 'k' is a new candidate right bound. 'k+1' a candidate left bound */
        if (k >= left)
         {
          if (sst[k] == LEFT_BOUND)  sst[k] = SORTED, n_sorted++;
          else                       sst[k] = RIGHT_BOUND;
         }
        kp = k+1;
        if ((kp) <= right)
         {
          if (sst[kp] == RIGHT_BOUND) sst[kp] = SORTED, n_sorted++;
          else                        sst[kp] = LEFT_BOUND;
         }
       }
     }
   }
  free(sst);
  return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_append
 * job_registry_append_op
 *
 * Appends an entry to the registry, unless an entry with the same
 * active ID is found there already (in that case will fall through
 * to job_registry_update).
 * job_registry_append will cause job_registry_append_op to open, (write) lock 
 * and close the registry file, while job_registry_append_op will work
 * on a file that's aleady open and writelocked.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param entry pointer to a registry entry to append to the registry.
 *        entry->recnum, entry->cdate and entry->mdate will be updated
 *        to current values. entry->magic_start and entry->magic_end will
 *        be appropriately set.
 * @param fd Stream descriptor of an open (for write) and writelocked 
 *        registry file. The file will be opened and closed if fd==NULL.
 * @param now Timestamp to be used for the update.
 *
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */
int
job_registry_append(job_registry_handle *rha,
                    job_registry_entry *entry)
{
  return job_registry_append_op(rha, entry, NULL, time(0));
}

int
job_registry_append_op(job_registry_handle *rha,
                       job_registry_entry *entry, FILE *fd, time_t now)
{
  job_registry_recnum_t found;
  job_registry_entry last;
  long curr_pos;
  int need_to_fclose = FALSE;
  int ret;

  if (rha->mode != NO_INDEX)
   {
    if (rha->mode == BY_BLAH_ID) 
      found = job_registry_lookup_op(rha, entry->blah_id, fd);
    else if (rha->mode == BY_USER_PREFIX)
      found = job_registry_lookup_op(rha, entry->user_prefix, fd);
    else
      found = job_registry_lookup_op(rha, entry->batch_id, fd);
    if (found != 0)
     { 
      entry->recnum = found;
      return job_registry_update_op(rha, entry, TRUE, fd, JOB_REGISTRY_UPDATE_ALL);
     }
   }

  if (fd == NULL)
   {
    /* Open file, writelock it and append entry */

    fd = job_registry_open(rha,"a+");
    if (fd == NULL) return JOB_REGISTRY_FOPEN_FAIL;

    if (job_registry_wrlock(rha,fd) < 0)
     {
      fclose(fd);
      return JOB_REGISTRY_FLOCK_FAIL;
     }
    need_to_fclose = TRUE;
   }

  /* 'a+' positions the read pointer at the beginning of the file */
  /* Make sure we move there in case of an open fd. */
  if (fseek(fd, 0L, SEEK_END) < 0)
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_FSEEK_FAIL;
   }

  curr_pos = ftell(fd);
  if (curr_pos > 0)
   {
    /* Read in last recnum */
    if (fseek(fd, (long)-sizeof(job_registry_entry), SEEK_CUR) < 0)
     {
      if (need_to_fclose) fclose(fd);
      return JOB_REGISTRY_FSEEK_FAIL;
     }
    if (fread(&last, sizeof(job_registry_entry),1,fd) < 1)
     {
      if (need_to_fclose) fclose(fd);
      return JOB_REGISTRY_FREAD_FAIL;
     }
    /* Consistency checks */
    if ( (last.magic_start != JOB_REGISTRY_MAGIC_START) ||
         (last.magic_end   != JOB_REGISTRY_MAGIC_END) )
     {
      errno = ENOMSG;
      if (need_to_fclose) fclose(fd);
      return JOB_REGISTRY_CORRUPT_RECORD;
     }
    entry->recnum = last.recnum+1;
   }
  else entry->recnum = 1;

  entry->magic_start = JOB_REGISTRY_MAGIC_START;
  entry->magic_end   = JOB_REGISTRY_MAGIC_END;
  entry->reclen = sizeof(job_registry_entry);
  entry->cdate = entry->mdate = now;
  
  if (fwrite(entry, sizeof(job_registry_entry),1,fd) < 1)
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_FWRITE_FAIL;
   }

  ret = job_registry_resync(rha,fd);
  if (need_to_fclose) fclose(fd);
  return ret;
}

/*
 * job_registry_get_new_npufd
 *
 * Open a unique file for storage or retrieval of
 * non-privileged-update entries.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 *
 * @return Stream descriptor to open file, NULL (and errno set to
 *         ENOMEM) when running out of memory, NULL (and errno set
 *         appropriately) when file operations fail..
 */
FILE*
job_registry_get_new_npufd(job_registry_handle *rha)
{
  FILE *rfd = NULL;
  int lfd;
  char *tp;
  const char *npu_tail="/npu_XXXXXX";

  /* Append a filename to rha->npudir, so it can be passed back to */
  /* jobregistry_construct_path */
  tp = (char *)malloc(strlen(rha->npudir) + strlen(npu_tail) + 1);
  if (tp == NULL)
   {
    errno = ENOMEM;
    return NULL;
   }

  sprintf(tp, "%s%s", rha->npudir, npu_tail);
  lfd = mkstemp(tp);
  if (lfd >= 0) 
   {
    /* Make it group writable as usual */
    fchmod(lfd, 0664);
    rfd = fdopen(lfd, "w"); 
   }

  free (tp);

  return(rfd);
}

/*
 * job_registry_append_nonpriv
 *
 * Schedule an entry for appending it into the registry. This is done
 * by writing the entry as a file in rha->npudir.
 * The entry will be appended on the first lookup done by a
 * process that owns write rights to teh registry.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param entry pointer to a registry entry to append to the registry.
 *
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */
int
job_registry_append_nonpriv(job_registry_handle *rha,
                            job_registry_entry *entry)
{
    FILE *cfd;

    cfd = job_registry_get_new_npufd(rha);

    if (cfd == NULL) return JOB_REGISTRY_FOPEN_FAIL;

    entry->magic_start = JOB_REGISTRY_MAGIC_START;
    entry->magic_end   = JOB_REGISTRY_MAGIC_END;
    entry->reclen = sizeof(job_registry_entry);
    entry->cdate = entry->mdate = time(0);

    /* We don't need to lock the file or to make sure some sort of atomic  */
    /* write is made. If an incomplete entry is read, the file will be     */
    /* left untouched and read again. */

    if (fwrite(entry,sizeof(job_registry_entry), 1, cfd) < 1)
     {
      fclose(cfd);
      return JOB_REGISTRY_FWRITE_FAIL;
     }

    fclose(cfd);

    return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_merge_pending_nonpriv_updates
 *
 * Look for new entries that could not be appended to the registry
 * file due to lack of privileges and append them to the registry.
 *
 * NOTE: for non privileged updates, any check that the added entries
 *       are unique w.r.t. the current index/search key is dropped.
 *
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param fd Stream descriptor of an open and *write* locked 
 *        registry file. The registry file will be opened and closed if
 *        fd==NULL.
 *
 * @return Number of added entries. Less than zero on error. 
 *         See job_registry.h for error codes.
 *         errno is also set in case of error.
 */
int
job_registry_merge_pending_nonpriv_updates(job_registry_handle *rha,
                                           FILE *fd)
{
  int nadd = 0;
  int rapp;
  int frret;
  job_registry_entry en;
  FILE *ofd = NULL;
  struct stat cfp_st;
  char *cfp;
  FILE *cfd, *npsfd;
  DIR  *npd;
  struct dirent *de;
  char subline[JOB_REGISTRY_MAX_SUBJECTLIST_LINE];
  char *sp;
  job_registry_index_mode saved_rha_mode;
  off_t last_end;
  uint32_t saved_rha_lastrec;

  ofd = fd;

  npd = opendir(rha->npudir);
  if (npd == NULL)
   {
    return JOB_REGISTRY_OPENDIR_FAIL; 
   }
  
  saved_rha_mode = rha->mode;
  saved_rha_lastrec = rha->lastrec;

  while ((de = readdir(npd)) != NULL) 
   {
    cfp = (char *)malloc(strlen(rha->npudir)+strlen(de->d_name)+2);
    if (cfp != NULL)
     {
      sprintf(cfp,"%s/%s",rha->npudir,de->d_name);
      if (stat(cfp, &cfp_st) < 0)   { free(cfp); continue; }
      if (!S_ISREG(cfp_st.st_mode)) { free(cfp); continue; }
      cfd = fopen(cfp, "r");
      if (cfd == NULL) {
		  free(cfp);
		  continue;
	  }

      frret = job_registry_probe_next_record(cfd, &en);
      if (frret == 0)
       {
        /* Can't get anything out of this file. It could be in the  */
        /* process of being written */
        fclose(cfd);
        /* Get rid of the file only if it was last modified "long" time ago */
        if ((time(0) - cfp_st.st_mtime) > 
            JOB_REGISTRY_CORRUPTED_NPU_FILES_MAX_LIFETIME) 
          unlink(cfp);
        free(cfp);
        continue;
       }

      fclose(cfd);
      if (ofd == NULL)
       {
        /* Open file and writelock it */
        /* Don't user job_registry_open, as we get called from */
        /* within it. */

        ofd = fopen(rha->path,"a+");
        if (ofd == NULL) {
            free(cfp);
            closedir(npd);
            return JOB_REGISTRY_FOPEN_FAIL;
        }

        if (job_registry_wrlock(rha,ofd) < 0)
         {
          free(cfp);
          fclose(ofd);
          closedir(npd);
          return JOB_REGISTRY_FLOCK_FAIL;
         }
        /* Append nonpriv entries with no further check */
        job_registry_resync(rha, ofd);
        saved_rha_lastrec = rha->lastrec;
        rha->mode = NO_INDEX;
       }

      fseek(ofd, 0L, SEEK_END);
      last_end = ftell(ofd);
      /* Use the file mtime as event creation timestamp */
      if ((rapp = job_registry_append_op(rha, &en, ofd, cfp_st.st_mtime)) < 0) 
       {
        free(cfp);
        rha->mode = saved_rha_mode;
        rha->lastrec = saved_rha_lastrec;
        job_registry_resync(rha, ofd);
        if (fd == NULL && ofd != NULL) fclose(ofd);
        closedir(npd);
        return rapp;
       }
      else
       {
        /* With no entry uniqueness check, failing to unlink the */
        /* NPU file becomes a consistency error we have to avoid. */
        if (unlink(cfp) < 0)
         {
          int result;
          free(cfp);
          /* Undo the append while we still hold a write lock */
          result = ftruncate(fileno(ofd), last_end);
          assert(result == 0);
          rha->mode = saved_rha_mode;
          rha->lastrec = saved_rha_lastrec;
          job_registry_resync(rha, ofd);
          if (fd == NULL && ofd != NULL) fclose(ofd);
          closedir(npd);
          return JOB_REGISTRY_UNLINK_FAIL;
         }
        nadd++;
       }
      free(cfp);
     }
   }
  closedir(npd);
  /* Resync before (possibly) dropping the write lock */
  if (ofd != NULL)
   {
    rha->mode = saved_rha_mode;
    rha->lastrec = saved_rha_lastrec;
    job_registry_resync(rha, ofd);
   }
  if (fd == NULL && ofd != NULL) fclose(ofd);

  if (nadd > 0)
   {
    /* Try merging the non-privileged subject list, if any */
    npsfd = fopen(rha->npusubjectlist, "r+");
    if (npsfd != NULL && job_registry_wrlock(rha,npsfd) >= 0)
     {
      while (!feof(npsfd))
       {
        if (fgets(subline, sizeof(subline), npsfd) != NULL)
         {
          sp = strchr(subline, ' ');
          if (sp == NULL) continue;
          *sp = '\000';
          sp++;
          if (sp[strlen(sp)-1] == '\n') sp[strlen(sp)-1] = '\000';
          job_registry_record_subject_hash(rha, subline, sp, FALSE);
         }
       }
      unlink(rha->npusubjectlist);
     }
    if (npsfd != NULL) fclose(npsfd);
   }

  return nadd;
}

/*
 * job_registry_update
 * job_registry_update_select
 * job_registry_update_recn
 * job_registry_update_recn_select
 * job_registry_update_op
 *
 * Update an existing entry in the job registry pointed to by rha.
 * Will return JOB_REGISTRY_NOT_FOUND if the entry does not exist.
 * job_registry_update will cause job_registry_update_op to open, (write) lock 
 * and close the registry file, while job_registry_update_op will work
 * on a file that's aleady open and writelocked.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param entry pointer to a registry entry with values to be
 *        updated into the registry.
 *        The values of udate, status, exitcode, exitreason, wn_addr
 *        and updater_info will be used for the update.
 *        The entry is updated with the actual registry contents upon
 *        successful return.
 * @param recn Cause an update of the specified record number.
 * @param use_recn If TRUE use the recnum field in 'entry' instead 
 *        of an index lookup operation.
 * @param fd Stream descriptor of an open (for write) and writelocked 
 *        registry file. The file will be opened and closed if fd==NULL.
 * @param upbits Bitmask selecting which registry fields should get updated by
 *        job_registry_update_op. Or'ed combination of the following:
 *         - JOB_REGISTRY_UPDATE_WN_ADDR 
 *         - JOB_REGISTRY_UPDATE_STATUS  
 *         - JOB_REGISTRY_UPDATE_EXITCODE 
 *         - JOB_REGISTRY_UPDATE_UDATE 
 *         - JOB_REGISTRY_UPDATE_EXITREASON 
 *         - JOB_REGISTRY_UPDATE_UPDATER_INFO
 *         or JOB_REGISTRY_UPDATE_ALL for all of the above fields.
 *
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error. JOB_REGISTRY_UNCHANGED (>0)
 *         is returned if the update operation caused no change. 
 */

int
job_registry_update_select(job_registry_handle *rha,
                           job_registry_entry *entry,
                           job_registry_update_bitmask_t upbits)
{
  return job_registry_update_op(rha, entry, FALSE, NULL, upbits);
}

int
job_registry_update(job_registry_handle *rha,
                    job_registry_entry *entry)
{
  return job_registry_update_op(rha, entry, FALSE, NULL, JOB_REGISTRY_UPDATE_ALL);
}

int
job_registry_update_recn_select(job_registry_handle *rha,
                           job_registry_entry *entry,
                           job_registry_recnum_t recn,
                           job_registry_update_bitmask_t upbits)
{
  entry->recnum = recn;
  return job_registry_update_op(rha, entry, TRUE, NULL, upbits);
}

int
job_registry_update_recn(job_registry_handle *rha,
                         job_registry_entry *entry,
                         job_registry_recnum_t recn)
{
  entry->recnum = recn;
  return job_registry_update_op(rha, entry, TRUE, NULL, JOB_REGISTRY_UPDATE_ALL);
}

int
job_registry_update_op(job_registry_handle *rha,
                       job_registry_entry *entry, int use_recn, FILE *fd,
                       job_registry_update_bitmask_t upbits)
{
  job_registry_recnum_t found, firstrec, req_recn;
  job_registry_entry old_entry;
  int need_to_fclose = FALSE;
  int need_to_update = FALSE;
  int update_binfo_only = TRUE;
  int retcod;

  if (use_recn)
   {
    found = entry->recnum;
   }
  else
   {
    if (rha->mode == NO_INDEX)
      return JOB_REGISTRY_NO_INDEX;
    else if (rha->mode == BY_BLAH_ID) 
      found = job_registry_lookup_op(rha, entry->blah_id, fd);
    else if (rha->mode == BY_USER_PREFIX) 
      found = job_registry_lookup_op(rha, entry->user_prefix, fd);
    else
      found = job_registry_lookup_op(rha, entry->batch_id, fd);
    if (found == 0)
     {
      return JOB_REGISTRY_NOT_FOUND;
     }
   }

  if (fd == NULL)
   {
    /* Open file, writelock it and replace entry */

    fd = job_registry_open(rha,"r+");
    if (fd == NULL) return JOB_REGISTRY_FOPEN_FAIL;

    if (job_registry_wrlock(rha,fd) < 0)
     {
      fclose(fd);
      return JOB_REGISTRY_FLOCK_FAIL;
     }
    need_to_fclose = TRUE;
   }

  firstrec = job_registry_firstrec(rha,fd);
  /* Was this record just purged ? */
  if ((firstrec > rha->firstrec) && (found >= rha->firstrec) && (found < firstrec))
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_NOT_FOUND;
   }
  JOB_REGISTRY_GET_REC_OFFSET(req_recn,found,firstrec)
  
  if (fseek(fd, (long)(req_recn*sizeof(job_registry_entry)), SEEK_SET) < 0)
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_FSEEK_FAIL;
   }
  if (fread(&old_entry, sizeof(job_registry_entry),1,fd) < 1)
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_FREAD_FAIL;
   }
  if (old_entry.recnum != found)
   {
    errno = EBADMSG;
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_BAD_RECNUM;
   }
  if (fseek(fd, (long)(req_recn*sizeof(job_registry_entry)), SEEK_SET) < 0)
   {
    if (need_to_fclose) fclose(fd);
    return JOB_REGISTRY_FSEEK_FAIL;
   }

  /* Update original entry and rewrite it */
  /* Warning: these checks should all be mirrored in job_registry_need_update */
  /*          below */
  if (((upbits & JOB_REGISTRY_UPDATE_WN_ADDR) != 0) &&
      (strncmp(old_entry.wn_addr, entry->wn_addr, 
               sizeof(old_entry.wn_addr)) != 0))
   {
    JOB_REGISTRY_ASSIGN_ENTRY(old_entry.wn_addr, entry->wn_addr);
    need_to_update = TRUE;
    update_binfo_only = FALSE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_STATUS) != 0) &&
       (old_entry.status != entry->status))
   {
    old_entry.status = entry->status;
    need_to_update = TRUE;
    update_binfo_only = FALSE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_EXITCODE) != 0) &&
       (old_entry.exitcode != entry->exitcode))
   {
    old_entry.exitcode = entry->exitcode;
    need_to_update = TRUE;
    update_binfo_only = FALSE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_UDATE) != 0) &&
       (old_entry.udate != entry->udate))
   {
    old_entry.udate = entry->udate;
    need_to_update = TRUE;
    update_binfo_only = FALSE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_EXITREASON) != 0) &&
       (strncmp(old_entry.exitreason, entry->exitreason, 
                sizeof(old_entry.exitreason)) != 0))
   {
    JOB_REGISTRY_ASSIGN_ENTRY(old_entry.exitreason, entry->exitreason);
    need_to_update = TRUE;
    update_binfo_only = FALSE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_UPDATER_INFO) != 0) &&
       (strncmp(old_entry.updater_info, entry->updater_info, 
                sizeof(old_entry.updater_info)) != 0))
   {
    JOB_REGISTRY_ASSIGN_ENTRY(old_entry.updater_info, entry->updater_info);
    need_to_update = TRUE;
   }
  else update_binfo_only = FALSE;

  retcod = JOB_REGISTRY_UNCHANGED;

  if (need_to_update)
   {
    if (! update_binfo_only) old_entry.mdate = time(0);
  
    if (fwrite(&old_entry, sizeof(job_registry_entry),1,fd) < 1)
     {
      if (need_to_fclose) fclose(fd);
      return JOB_REGISTRY_FWRITE_FAIL;
     }
    else
     {
      if (update_binfo_only) retcod = JOB_REGISTRY_BINFO_ONLY;
      else                   retcod = JOB_REGISTRY_SUCCESS;
     }
   }

  if (need_to_fclose) fclose(fd);

  /* Update entry contents with actual registry entry */
  memcpy(entry, &old_entry, sizeof(job_registry_entry));

  return retcod;
}

/*
 * job_registry_need_update
 *
 * Check two job registry entries, an 'old' and a 'new' one, and
 * check whether job_registry_update would need to actually write
 * an update to disk. This function is meant to save the unneeded
 * acquisition of a write lock.
 *
 * @param olde pointer to an existing registry entry.
 * @param newe pointer to a possibly updated registry entry.
 * @param upbits Bitmask selecting which registry fields should be
 *               checked. Or'ed combination of the following:
 *         - JOB_REGISTRY_UPDATE_WN_ADDR 
 *         - JOB_REGISTRY_UPDATE_STATUS  
 *         - JOB_REGISTRY_UPDATE_EXITCODE 
 *         - JOB_REGISTRY_UPDATE_UDATE 
 *         - JOB_REGISTRY_UPDATE_EXITREASON 
 *         - JOB_REGISTRY_UPDATE_UPDATER_INFO
 *         or JOB_REGISTRY_UPDATE_ALL for all of the above fields.
 *
 * @return Boolean valued integer. TRUE (nonzero) if an update is
 *         actually needed.
 */

int
job_registry_need_update(const job_registry_entry *olde,
                         const job_registry_entry *newe,
                         job_registry_update_bitmask_t upbits)
{
  int need_to_update = FALSE;

  if (((upbits & JOB_REGISTRY_UPDATE_WN_ADDR) != 0) &&
      (strncmp(olde->wn_addr, newe->wn_addr, 
               sizeof(olde->wn_addr)) != 0))
   {
    need_to_update = TRUE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_STATUS) != 0) &&
       (olde->status != newe->status))
   {
    need_to_update = TRUE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_EXITCODE) != 0) &&
       (olde->exitcode != newe->exitcode))
   {
    need_to_update = TRUE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_UDATE) != 0) &&
       (olde->udate != newe->udate))
   {
    need_to_update = TRUE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_EXITREASON) != 0) &&
       (strncmp(olde->exitreason, newe->exitreason, 
                sizeof(olde->exitreason)) != 0))
   {
    need_to_update = TRUE;
   }
  if (((upbits & JOB_REGISTRY_UPDATE_UPDATER_INFO) != 0) &&
       (strncmp(olde->updater_info, newe->updater_info, 
                sizeof(olde->updater_info)) != 0))
   {
    need_to_update = TRUE;
   }
  return need_to_update;
}

/*
 * job_registry_get_recnum
 *
 * Binary search for an entry in the indexed, sorted job registry pointed to by
 * rha. The record number in the current JR cache is returned.
 * No file access is required.
 * In case multiple entries are found, the highest (most recent) recnum 
 * is returned.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param id Job id key to be looked up 
 *
 * @return Record number of the found record, or 0 if the record was not found.
 */

job_registry_recnum_t 
job_registry_get_recnum(const job_registry_handle *rha,
                        const char *id)
{
  /* Binary search in entries */
  int left,right,cur,tcur;
  job_registry_recnum_t found=0;
  int cmp;

  left = 0;
  right = rha->n_entries -1;

  while (right >= left)
   {
    cur = (right + left) /2;
    cmp = strcmp(rha->entries[cur].id,id);
    if (cmp == 0)
     {
      found = rha->entries[cur].recnum;
      /* Check for duplicates. */
      for (tcur=cur-1; tcur >=0 && strcmp(rha->entries[tcur].id,id)==0; tcur--)
       {
        if (rha->entries[tcur].recnum > found) found = rha->entries[tcur].recnum;
       }
      for (tcur=cur+1;tcur < rha->n_entries && 
                             strcmp(rha->entries[tcur].id,id)==0; tcur++)
       {
        if (rha->entries[tcur].recnum > found) found = rha->entries[tcur].recnum;
       }
      break;
     }
    else if (cmp < 0)
     {
      left = cur+1;
     }
    else
     {
      right = cur-1;
     }
   }
  return found;
}

/*
 * job_registry_lookup
 * job_registry_lookup_op
 *
 * Binary search for an entry in the indexed, sorted job registry pointed to by
 * rha. If the entry is not found, a job_registry_resync will be attempted.
 * job_registry_lookup_op can operate on an already open and at least read-
 * locked file.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param id Job id key to be looked up 
 * @param fd Stream descriptor of an open and *write* locked 
 *        registry file. The write lock is needed only in case
 *        non privileged new entries need to be merged into the registry.
 *        When fd==NULL, the file will be opened and closed if new 
 *        non-privileged entries are present (write lock), or 
 *        job_registry_resync is needed (read lock).
 *
 * @return Record number of the found record, or 0 if the record was not found.
 */

job_registry_recnum_t 
job_registry_lookup(job_registry_handle *rha,
                    const char *id)
{
  /* As a resync attempt is performed inside this function, */
  /* the registry should not be open when this function is called. */
  /* Use job_registry_lookup_op if an open file is needed */

  return job_registry_lookup_op(rha, id, NULL);
}

job_registry_recnum_t 
job_registry_lookup_op(job_registry_handle *rha,
                       const char *id, FILE *fd)
{
  job_registry_recnum_t found=0;
  int retry;
  int need_to_fclose = FALSE;

  for (retry=0; retry < 2; retry++)
   {
    found = job_registry_get_recnum(rha, id); 

    /* If the entry was not found the first time around, try resyncing once */
    if (found == 0 && retry == 0)
     {
      if (fd == NULL)
       {
        fd = job_registry_open(rha, "r");
        if (fd == NULL) break;
        if (job_registry_rdlock(rha, fd) < 0)
         {
          fclose(fd);
          break;
         }
        need_to_fclose = TRUE;
       }
      if (job_registry_resync(rha, fd) <= 0)
       {
        if (need_to_fclose) fclose(fd);
        break;
       }
      if (need_to_fclose) fclose(fd);
     }
    if (found > 0) break;
   }
  return found;
}

/*
 * job_registry_get
 *
 * Search for an entry in the indexed, sorted job registry pointed to by
 * rha and fetch it from the registry file. If the entry is not found, a 
 * job_registry_resync will be attempted.
 * The registry file will be opened, locked and closed as an effect
 * of this operation, so the file should not be open upon entering
 * this function.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param id Job id key to be looked up 
 *
 * @return Dynamically allocated registry entry. Needs to be free'd.
 */

job_registry_entry *
job_registry_get(job_registry_handle *rha,
                 const char *id)
{
  job_registry_recnum_t found, firstrec, req_recn;
  job_registry_entry *entry;
  FILE *fd;

  if (rha->mode == NO_INDEX)
   {
    errno = EINVAL;
    return NULL;
   }

  found = job_registry_lookup(rha, id);
  if (found == 0)
   {
    errno = ENOENT;
    return NULL;
   }

  /* Open file, readlock it and fetch entry */

  fd = job_registry_open(rha,"r");
  if (fd == NULL) return NULL;

  if (job_registry_rdlock(rha,fd) < 0)
   {
    fclose(fd);
    return NULL;
   }

  firstrec = job_registry_firstrec(rha,fd);

  /* Determine if the job registry index must be resync'd.
   * The record numbers are monotonically increasing through the lifetime
   * of the registry; the firstrec we read from the data file above must
   * match the firstrec in our in-memory index.  The firstrec on the index
   * is guaranteed to change if a purge operation occurred.
   */
  if (firstrec != rha->firstrec)
   {
    int retval = job_registry_resync(rha, fd);
    if (retval < 0)  // Registry failed to update.
     {
      fclose(fd);
      return NULL;
     }
    if (retval > 0)  // Registry has been updated; our lookup was invalid.
     {
      found = job_registry_lookup(rha, id);
      if (found == 0)
       {
        errno = ENOENT;
        fclose(fd);
        return NULL;
       }
     }
   }

  /* Was this record just purged ? */
  if ((firstrec > rha->firstrec) && (found >= rha->firstrec) && (found < firstrec))
   {
    fclose(fd);
    return NULL;
   }
  JOB_REGISTRY_GET_REC_OFFSET(req_recn,found,firstrec)
  
  if (fseek(fd, (long)(req_recn*sizeof(job_registry_entry)), SEEK_SET) < 0)
   {
    fclose(fd);
    return NULL;
   }
  
  entry = job_registry_get_next(rha, fd);

  fclose(fd);
  return entry;
}

/*
 * job_registry_open
 *
 * Open a registry file. Just a wrapper around fopen at the time being.
 * May include more operations further on.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param mode fopen mode string.
 *
 * @return stream descriptor of the open file or NULL on error.
 */

FILE *
job_registry_open(job_registry_handle *rha, const char *mode)
{
  FILE *fd;

  /* Do we need to merge any non-privileged updates into the file? */
  job_registry_merge_pending_nonpriv_updates(rha, NULL);

  fd = fopen(rha->path, mode);

  return fd;
}

/*
 * job_registry_unlock
 *
 * Release any lock on open file sfd. This is useful to yield
 * permission to other processes on long read cycles.
 *
 * @param sfd Stream descriptor of an open where fcntl locks are released.
 * 
 * @return Less than zero on error (errno is set by fcntl call).
 */

int
job_registry_unlock(FILE *sfd)
{
  int fd;
  struct flock ulock;
  int ret;

  fd = fileno(sfd);
  if (fd < 0) return fd; /* sfd is an invalid stream */

  /* Now release any lock on the whole file */

  ulock.l_type = F_UNLCK;
  ulock.l_whence = SEEK_SET;
  ulock.l_start = 0;
  ulock.l_len = 0; /* Lock whole file */
  
  ret = fcntl(fd, F_SETLKW, &ulock);
  return ret;
}

/*
 * job_registry_rdlock
 *
 * Obtain a read lock to sfd, after making sure no write lock request
 * is pending on the same file.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param sfd Stream descriptor of an open (for reading) file to lock.
 * 
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_rdlock(const job_registry_handle *rha, FILE *sfd)
{
  int fd, lfd;
  struct flock tlock, rlock;
  int ret;
  mode_t old_umask;

  fd = fileno(sfd);
  if (fd < 0) return fd; /* sfd is an invalid stream */

  /* First of all, try obtaining a write lock to rha->lockfile */
  /* to make sure no write lock is pending */

  old_umask = umask(0);
  lfd = open(rha->lockfile, O_WRONLY|O_CREAT, 
             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH); 
  umask(old_umask);
  if (lfd < 0) return lfd;

  tlock.l_type = F_WRLCK;
  tlock.l_whence = SEEK_SET;
  tlock.l_start = 0;
  tlock.l_len = 0; /* Lock whole file */

  if ((ret = fcntl(lfd, F_SETLKW, &tlock)) < 0) 
   {
    close(lfd);
    return ret;
   }

  /* Close file immediately */

  close(lfd);

  /* Now obtain the requested read lock */

  rlock.l_type = F_RDLCK;
  rlock.l_whence = SEEK_SET;
  rlock.l_start = 0;
  rlock.l_len = 0; /* Lock whole file */
  
  ret = fcntl(fd, F_SETLKW, &rlock);
  return ret;
}

/*
 * job_registry_wrlock
 * 
 * Obtain a write lock to sfd. Also lock rha->lockfile to prevent more
 * read locks from being issued.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param sfd Stream descriptor of an open (for writing) file to lock.
 * 
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_wrlock(const job_registry_handle *rha, FILE *sfd)
{
  int fd, lfd;
  struct flock tlock, wlock;
  int ret;
  mode_t old_umask;

  fd = fileno(sfd);

  /* Obtain and keep a write lock to rha->lockfile */
  /* to prevent new read locks. */

  old_umask = umask(0);
  lfd = open(rha->lockfile, O_WRONLY|O_CREAT,
             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH); 
  umask(old_umask);
  if (lfd < 0) return lfd;

  tlock.l_type = F_WRLCK;
  tlock.l_whence = SEEK_SET;
  tlock.l_start = 0;
  tlock.l_len = 0; /* Lock whole file */

  if ((ret = fcntl(lfd, F_SETLKW, &tlock)) < 0)
   {
    close(lfd);
    return ret;
   }

  /* Make sure the world-writable lock file continues to be empty */
  /* We check this when obtaining registry write locks only */
  ret = ftruncate(lfd, 0);
  if (ret < 0) {
    close(lfd); // also releases lock on lfd
    return ret;
  }

  /* Now obtain the requested write lock */

  wlock.l_type = F_WRLCK;
  wlock.l_whence = SEEK_SET;
  wlock.l_start = 0;
  wlock.l_len = 0; /* Lock whole file */
  
  ret = fcntl(fd, F_SETLKW, &wlock);

  /* Release lock on rha->lockfile */

  close(lfd);

  return ret;
}

/*
 * job_registry_get_next
 *
 * Get the registry entry currently pointed to in open stream fd and try
 * making sure it is consistent.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param fd Open (at least for reading) stream descriptor into a registry file
 *        The stream must be positioned at the beginning of a valid entry,
 *        or an error will be returned. Use job_registry_seek_next is the
 *        stream needs to be positioned.
 *
 * @return Dynamically allocated registry entry. Needs to be free'd.
 */

job_registry_entry *
job_registry_get_next(const job_registry_handle *rha,
                      FILE *fd)
{
  int ret;
  job_registry_entry *result=NULL;
  long curr_pos;
  job_registry_recnum_t curr_recn;

  result = (job_registry_entry *)malloc(sizeof(job_registry_entry));
  if (result == NULL)
   {
    errno = ENOMEM;
    return NULL;
   } 
  
  ret = fread(result, sizeof(job_registry_entry), 1, fd);
  if (ret < 1)
   {
    free(result);
    return NULL;
   }
  if ( (result->magic_start != JOB_REGISTRY_MAGIC_START) ||
       (result->magic_end   != JOB_REGISTRY_MAGIC_END) )
   {
    errno = ENOMSG;
    free(result);
    return NULL;
   }

  if (rha->disk_firstrec > 0) /* Keep checking file consistency */
                              /* (correspondence of file offset and record num) */
   {
    curr_pos = ftell(fd);
    JOB_REGISTRY_GET_REC_OFFSET(curr_recn,result->recnum,rha->disk_firstrec)
    if (curr_pos != (long)((curr_recn+1)*sizeof(job_registry_entry)))
     {
      errno = EBADMSG;
      free(result);
      return NULL;
     }
   }
  return result;
}

/*
 * job_registry_seek_next
 *
 * Look for a valid registry entry anywhere starting from the current 
 * position in fd. No consistency check is performed. 
 * This function can be used for file recovery. 
 *
 * @param fd Open (at least for reading) stream descriptor into a registry file.
 * @param result Pointer to an allocated registry entry that will be 
 *        used to store the result of the read.
 *
 * @return Less than zero on error. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_seek_next(FILE *fd, job_registry_entry *result)
{
  int ret;
  
  ret = fread(result, sizeof(job_registry_entry), 1, fd);
  if (ret < 1)
   {
    if (feof(fd)) return 0;
    else          return JOB_REGISTRY_FREAD_FAIL;
   }

  while ( (result->magic_start != JOB_REGISTRY_MAGIC_START) ||
          (result->magic_end   != JOB_REGISTRY_MAGIC_END) )
   {
    /* Move 1 byte ahead */
    ret = fseek(fd, (long)(-sizeof(job_registry_entry)+1), SEEK_CUR);
    if (ret < 0)
     {
      return JOB_REGISTRY_FSEEK_FAIL;
     }

    ret = fread(result, sizeof(job_registry_entry), 1, fd);
    if (ret < 1)
     {
      if (feof(fd)) return 0;
      else          return JOB_REGISTRY_FREAD_FAIL;
     }
   }

  return 1;
}

/*
 * job_registry_entry_as_classad
 *
 * Create a classad (in standard string representation)
 * including the attributes of the supplied registry entry.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param entry Job registry entry to be formatted as classad.
 *
 * @return Dynamically-allocated string containing the classad
 *         rappresentation of entry.
 */

#define JOB_REGISTRY_APPEND_ATTRIBUTE(format,attribute) \
    fmt_extra = (format); \
    esiz = snprintf(NULL, 0, fmt_extra, (attribute)) + 1; \
    new_extra_attrs = (char *)realloc(extra_attrs, extra_attrs_size + esiz); \
	assert(new_extra_attrs); \
    need_to_free_extra_attrs = TRUE; \
    extra_attrs = new_extra_attrs; \
    snprintf(extra_attrs+extra_attrs_size, esiz, fmt_extra, (attribute)); \
    extra_attrs_size += (esiz-1); 

char *
job_registry_entry_as_classad(const job_registry_handle *rha,
                              const job_registry_entry *entry)
{
  char *fmt_base = "[ BatchJobId=\"%s\"; JobStatus=%d; BlahJobId=\"%s\"; "
                   "CreateTime=%u; ModifiedTime=%u; UserTime=%u; "
                   "SubmitterUid=%d; %s]";
  char *result, *fmt_extra, *extra_attrs=NULL, *new_extra_attrs;
  int extra_attrs_size = 0;
  int need_to_free_extra_attrs = FALSE;
  int esiz,fsiz;
  char *proxypath;

  if (strlen(entry->wn_addr) > 0) 
   { 
    JOB_REGISTRY_APPEND_ATTRIBUTE("WorkerNode=\"%s\"; ",entry->wn_addr);
   }
  if (strlen(entry->proxy_link) > 0) 
   { 
    proxypath = job_registry_get_proxy(rha, entry);
    if (proxypath != NULL)
     {
      JOB_REGISTRY_APPEND_ATTRIBUTE("X509UserProxy=\"%s\"; ",proxypath);
      free(proxypath);
     }
   }
  if (entry->status == COMPLETED)
   {
    JOB_REGISTRY_APPEND_ATTRIBUTE("ExitCode=%d; ",entry->exitcode);
   }
  if (strlen(entry->exitreason) > 0) 
   { 
    JOB_REGISTRY_APPEND_ATTRIBUTE("ExitReason=\"%s\"; ",entry->exitreason);
   }
  if (strlen(entry->user_prefix) > 0) 
   { 
    JOB_REGISTRY_APPEND_ATTRIBUTE("UserPrefix=\"%s\"; ",entry->user_prefix);
   }

  if (extra_attrs == NULL) 
   {
    extra_attrs = "";
    need_to_free_extra_attrs = FALSE;
   }

  fsiz = snprintf(NULL, 0, fmt_base, 
                  entry->batch_id, entry->status, entry->blah_id,
                  entry->cdate, entry->mdate, entry->udate, entry->submitter,
                  extra_attrs) + 1;

  result = (char *)malloc(fsiz);
  if (result)
    snprintf(result, fsiz, fmt_base,
             entry->batch_id, entry->status, entry->blah_id,
             entry->cdate, entry->mdate, entry->udate, entry->submitter,
             extra_attrs);

  if (need_to_free_extra_attrs) free(extra_attrs);

  return result;
}

/*
 * job_registry_split_blah_id
 *
 * Return a structure (to be freed with job_registry_free_split_id)
 * filled with dynamically allocated copies of the various parts
 * of the BLAH ID:
 *  - lrms: the part up to and excluding the first slash ('/')
 *  - script_id: the part following and excluding the first slash 
 *  - proxy_id: the part between the first and the second slash 
 *              for the 'lrms==condor' case. After the last
 *              slash otherwise.
 *
 * @param id BLAH job ID string.
 *
 * @return Pointer to a job_registry_split_id structure containing the split id
 *         or NULL if no slash was found in the ID or malloc failed.
 *         The resulting pointer has to be freed via job_registry_free_split_id.
 */

job_registry_split_id *
job_registry_split_blah_id(const char *bid)
 {
  const char *firsts, *seconds, *pstart;
  int fsl, ssl, psl;
  job_registry_split_id *ret;

  firsts = strchr(bid, '/');

  if (firsts == NULL) return NULL;

  ret = (job_registry_split_id *)malloc(sizeof(job_registry_split_id));
  if (ret == NULL) return NULL;

  fsl = (int)(firsts - bid);
  ret->lrms = (char *)malloc(fsl + 1);
  if (ret->lrms == NULL)
   {
    job_registry_free_split_id(ret);
    return NULL;
   }
  memcpy(ret->lrms, bid, fsl);
  (ret->lrms)[fsl] = '\000';

  /* FIXME: Work around syntax for Condor */
  
  if (strstr(ret->lrms, "con") == ret->lrms)
   {
    seconds = strchr(firsts+1, '/');
    if (seconds == NULL) seconds = bid+strlen(bid);
    pstart = firsts+1;
    psl = (int)(seconds - firsts) - 1;
   }
  else
   {
    seconds = strrchr(firsts+1, '/');
    if (seconds == NULL) seconds = firsts;
    pstart = seconds+1;
    psl = strlen(pstart);
   }

  ssl = strlen(bid)-fsl-1;

  ret->script_id = (char *)malloc(ssl + 1);
  ret->proxy_id  = (char *)malloc(psl + 1);

  if (ret->script_id == NULL || ret->proxy_id == NULL)
   {
    job_registry_free_split_id(ret);
    return NULL;
   }

  memcpy(ret->script_id, firsts+1, ssl);
  memcpy(ret->proxy_id,  pstart, psl);

  (ret->script_id)[ssl] = '\000';
  (ret->proxy_id) [psl] = '\000';

  return ret;
 }

/*
 * job_registry_free_split_id
 *
 * Frees a job_registry_split_id with all its dynamic contents.
 *
 * @param spid Pointer to a job_registry_split_id returned by 
 *             job_registry_split_blah_id.
 */

void 
job_registry_free_split_id(job_registry_split_id *spid)
 {
   if (spid == NULL) return;

   if (spid->lrms != NULL)      free(spid->lrms);
   if (spid->script_id != NULL) free(spid->script_id);
   if (spid->proxy_id != NULL)  free(spid->proxy_id);

   free(spid);
 }

/*
 * job_registry_set_proxy
 *
 * Add a proxy link to the current entry.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param en Pointer to a job registry entry.
 * @param proxy Path to the proxy to be registered
 *
 * @return Numeric status. < 0 in case of failure.
 */

int
job_registry_set_proxy(const job_registry_handle *rha,
                       job_registry_entry *en,
                       char *proxy)
{
  const char *proxylink_fmt="proxy_XXXXXX";
  const char *proxylink_id_fmt="proxy_%*.*s_XXXXXX";
  char *fullpath;
  size_t idlen;
  int spret = -1;
  int syret;
  int lfd;
  size_t i,j;

  if ((idlen = strlen(en->batch_id)) > 0)
   {
     if (idlen > 20) idlen = 20;
     spret = snprintf(en->proxy_link, sizeof(en->proxy_link), 
                      proxylink_id_fmt, idlen, idlen, en->batch_id);
     /* No slashes allowed here */
     for(i=0; i<strlen(en->proxy_link); i++)
      {
       if (en->proxy_link[i] == '/') en->proxy_link[i] = '_';
      }
   }

  if (spret < 0)
   {
     strncpy(en->proxy_link, proxylink_fmt, sizeof(en->proxy_link));
   }
  en->proxy_link[sizeof(en->proxy_link)-1] = '\000';

  fullpath = (char *)malloc(strlen(en->proxy_link) + strlen(rha->proxydir) + 2);
  if (fullpath == NULL)
   {
    en->proxy_link[0]='\000';
    errno = ENOMEM;
    return -1;
   }
  
  sprintf(fullpath, "%s/%s", rha->proxydir, en->proxy_link);

  if ((lfd = mkstemp(fullpath)) < 0)
   {
    free(fullpath);
    return lfd;
   }
   
  unlink(fullpath);
  syret = symlink(proxy, fullpath);

  close(lfd);

  if (syret >= 0)
   {
    /* Copy the unique part of the file into the entry */
    for (i = strlen(fullpath) - 6, j = strlen(en->proxy_link) - 6;
         i < strlen(fullpath); i++, j++)
     {
      en->proxy_link[j] = fullpath[i];
     }
   }
  else
   {
    en->proxy_link[0]='\000';
   }
  
  free(fullpath);
  return syret;
}

/*
 * job_registry_get_proxy
 *
 * Get the full pathname of the proxy linked to the current entry, if any.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param en Pointer to a job registry entry.
 *
 * @return Dynamically allocated path to the proxy (needs to be freed)
 *         or NULL in case of failure.
 */

char *
job_registry_get_proxy(const job_registry_handle *rha,
                       const job_registry_entry *en)
{
  char *fullpath;
  char *retl = NULL;
  char *new_retl;
  int retll;
  int rlret;

  fullpath = (char *)malloc(strlen(en->proxy_link) + strlen(rha->proxydir) + 2);
  if (fullpath == NULL)
   {
    errno = ENOMEM;
    return NULL;
   }
  
  sprintf(fullpath, "%s/%s", rha->proxydir, en->proxy_link);

  for (retll = 256; ;retll += 128)
   {
    new_retl = (char *)realloc(retl, retll);
    if (new_retl == NULL)
     {
      errno = ENOMEM;
      free(retl);
      retl = NULL;
      break;
     }

    retl = new_retl;
    rlret = readlink(fullpath, retl, retll); 
    if (rlret < 0)
     {
      free(retl);
      retl = NULL;
      break;
     }  
    if (rlret < retll)
     {
      retl[rlret]='\000'; /* readlink does not NULL-terminate */
      break;
     }
   }

  free(fullpath);
  return retl;
}

/*
 * job_registry_unlink_proxy
 *
 * Unlink and remove the proxy linked to the current entry, if any.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param en Pointer to a job registry entry.
 *
 * @return Numeric status: < 0 in case of failure. errno is set accordingly.
 */

int
job_registry_unlink_proxy(const job_registry_handle *rha,
                          job_registry_entry *en)
{
  char *fullpath;
  int unret;

  fullpath = (char *)malloc(strlen(en->proxy_link) + strlen(rha->proxydir) + 2);
  if (fullpath == NULL)
   {
    errno = ENOMEM;
    return -1;
   }
  
  sprintf(fullpath, "%s/%s", rha->proxydir, en->proxy_link);

  unret = unlink(fullpath);

  if (unret >= 0)
   {
    en->proxy_link[0]='\000';
   }

  free(fullpath);
  return unret;
}

/*
 * job_registry_compute_subject_hash
 *
 * Compute a MD5 hash out of a string containing a proxy certificate subject.
 *
 * @param en Pointer to a job registry entry. Its subject_hash will be
 *           modified to contain an MD5 hash of subject.
 * @param subject Pointer to a string containing the certificate subject.
 *
 */

void
job_registry_compute_subject_hash(job_registry_entry *en, const char *subject)
{
  md5_state_t ctx;
  md5_byte_t digest[16];
  char nibble[4];
  size_t i;

  if (en == NULL || subject == NULL) return;

  md5_init(&ctx);
  md5_append(&ctx, (const unsigned char*)subject, strlen(subject));
  md5_finish(&ctx, digest);

  if (sizeof(en->subject_hash) < 4) return;
  en->subject_hash[0] = '\000';

  for (i=0; i<16; i++)
   {
    if (i >= (sizeof(en->subject_hash)/2 - 3)) break;
    sprintf(nibble,"%02x",digest[i]);
    strcat(en->subject_hash, nibble);
   }
}

/*
 * job_registry_record_subject_hash
 *
 * Compute a MD5 hash out of a string containing a proxy certificate subject,
 * and record in a file archive the association between the MD5 hash
 * and the full proxy subject.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param hash    Pointer to a string containing the MD5 hash of subject.
 * @param subject Pointer to a string containing the certificate subject.
 *
 * @return 0 in case of success, < 0 in case of failure 
 *                               (errno is set accordingly).
 */

int
job_registry_record_subject_hash(const job_registry_handle *rha,
                                 const char *hash,
                                 const char *subject, 
                                 int nonpriv_allowed)
{
  FILE *fd;
  char subline[JOB_REGISTRY_MAX_SUBJECTLIST_LINE];
  int retcod;

  if (rha == NULL || hash == NULL || subject == NULL) return -1;

  fd = fopen(rha->subjectlist, "a+");
  if (fd == NULL)
   {
    if ((errno == EACCES) && nonpriv_allowed)
     {
      fd = fopen(rha->npusubjectlist, "a+");
      if (fd != NULL) 
       {
        /* Make sure other non-privileged appends to this file can be made */
        fchmod(fileno(fd), S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
       }
     }
   }

  if (fd == NULL) return JOB_REGISTRY_FOPEN_FAIL;

  if (job_registry_wrlock(rha, fd) < 0)
   {
    fclose(fd);
    return JOB_REGISTRY_FLOCK_FAIL;
   }

  while (!feof(fd))
   {
    if (fgets(subline, sizeof(subline), fd) != NULL)
     {
      if (strncmp(subline, hash, strlen(hash)) == 0)
       {
        /* Hash was found already. Right or wrong, keep this entry. */
        fclose(fd);
        if (strncmp(subline+strlen(hash)+1, subject, strlen(subject)) != 0)
         {
          return JOB_REGISTRY_HASH_EXISTS;
         }
        else return JOB_REGISTRY_SUCCESS;
       }
     }
   }
  /* If we reach here, we are at end-of-file, and we can append our entry */
  retcod = fprintf(fd, "%s %s\n", hash, subject);

  fclose(fd);
  if (retcod < 0) return JOB_REGISTRY_FWRITE_FAIL;
  return JOB_REGISTRY_SUCCESS;
}

/*
 * job_registry_lookup_subject_hash
 *
 * Find in a file archive (linear search) the full proxy subject cached 
 * under a given MD5 hash.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param hash    Pointer to a string containing the MD5 hash of subject.
 *
 * @return Pointer to dynamically allocated string holding the full
 *         subject name, if found. NULL in case of failure
 *         (errno is set accordingly).
 */

char *
job_registry_lookup_subject_hash(const job_registry_handle *rha,
                                 const char *hash)
{
  FILE *fd;
  char subline[JOB_REGISTRY_MAX_SUBJECTLIST_LINE];
  char *en;

  if (rha == NULL || hash == NULL) return NULL;

  fd = fopen(rha->subjectlist, "r");
  if (fd == NULL) return NULL;

  if (job_registry_rdlock(rha, fd) < 0)
   {
    fclose(fd);
    return NULL;
   }

  while (!feof(fd))
   {
    if (fgets(subline, sizeof(subline), fd) != NULL)
     {
      if (strncmp(subline, hash, strlen(hash)) == 0)
       {
        /* Found entry */
        en = strchr(subline, ' ');
        if (en == NULL) continue; /* Bogus entry ?! */
        en++;

        fclose(fd);
        if (en[strlen(en)-1] == '\n') en[strlen(en)-1] = '\000';
        return strdup(en);
       }
     }
   }

  fclose(fd);

  /* Could the entry be pending a privileged update ? */
  fd = fopen(rha->npusubjectlist, "r");
  if (fd == NULL) return NULL;

  if (job_registry_rdlock(rha, fd) < 0)
   {
    fclose(fd);
    return NULL;
   }

  while (!feof(fd))
   {
    if (fgets(subline, sizeof(subline), fd) != NULL)
     {
      if (strncmp(subline, hash, strlen(hash)) == 0)
       {
        /* Found entry */
        en = strchr(subline, ' ');
        if (en == NULL) continue; /* Bogus entry ?! */
        en++;

        fclose(fd);
        if (en[strlen(en)-1] == '\n') en[strlen(en)-1] = '\000';
        return strdup(en);
       }
     }
   }

  fclose(fd);
  return NULL;
}

/*
 * job_registry_get_next_hash_match
 *
 * Get the next registry entry in open stream fd whose 'subject_hash'
 * field matches the 'hash' argument. Try making sure it is consistent.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param fd Open (at least for reading) stream descriptor into a registry file
 *        The stream must be positioned at the beginning of a valid entry,
 *        or an error will be returned. Use job_registry_seek_next if the
 *        stream needs to be positioned.
 * @param hash string pointer to a MD5 hash of a proxy subject (as stored 
 *        in the subject_hash field of registry entries).
 *
 * @return Dynamically allocated registry entry. Needs to be free'd.
 */

job_registry_entry *
job_registry_get_next_hash_match(const job_registry_handle *rha,
                                 FILE *fd, const char *hash)
{
  if (rha == NULL || fd == NULL || hash == NULL) return NULL;

  job_registry_entry *result = NULL;

  for (;;) /* Will exit via break. */
   {
    result = job_registry_get_next(rha, fd);
    if (result == NULL) break;

    if (strncmp(result->subject_hash, hash, strlen(hash)) == 0) break;
    
    free(result);
   }
   
  return result;
}

/*
 * job_registry_store_hash
 *
 * Store a hash string in an ordered array for fast lookup.
 * This is used to purge the subjectlist.
 *
 * @param hst Pointer to a hash store handle. It will be initialised if 
 *        hst->data is NULL.
 * @param hash Hash string to store.
 *
 * @return <0 in case of failure (typically ENOMEM, as set by malloc). 
 */

int
job_registry_store_hash(job_registry_hash_store *hst,
                        const char *hash)
{
  char **new_data;
  char *hash_copy;
  char *swap1, *swap2;
  int i, insert;

  if (hst == NULL || hash == NULL) return -1;
   
  if (strlen(hash) <= 0) return 0;

  if (hst->data == NULL)
   {
    hst->data = (char **)malloc(sizeof(char *));
    if (hst->data == NULL)
     {
      hst->n_data = 0;
      return -1;
     }
    hst->data[0] = strdup(hash);
    if (hst->data[0] == NULL)
     {
      hst->n_data = 0;
      return -1;
     }
    hst->n_data = 1;
    return 0;
   }

  if (job_registry_lookup_hash(hst, hash, &insert) >= 0) return 0;

  new_data = realloc(hst->data , (hst->n_data+1) * sizeof(char *));
  assert(new_data);

  hash_copy = strdup(hash);
  assert(hash_copy);

  hst->data = new_data;
  
  swap1 = hst->data[insert];
  hst->data[insert] = hash_copy;

  for (i=insert+1; i<=hst->n_data; i++)
   {
    swap2 = hst->data[i];
    hst->data[i] = swap1;
    swap1 = swap2;
   }
  
  hst->n_data++;
  return 0;
}

/*
 * job_registry_lookup_hash
 *
 * Look a hash string up (binary search) in an ordered array.
 *
 * @param hst Pointer to a hash store handle. 
 * @param hash Hash string to look up.
 * @param loc Location of found record (or of insertion point
 *            of missing record). May be NULL if not needed;
 *
 * @return <0 if entry not found. Index of array entry if found. 
 */

int
job_registry_lookup_hash(const job_registry_hash_store *hst,
                         const char *hash,
                         int *loc)
{
  int left,right,cur;
  int found=-1;
  int cmp;

  if (hst == NULL || hash == NULL) return -1;

  if (loc != NULL) *loc = 0;

  left = 0;
  right = hst->n_data -1;

  while (right >= left)
   {
    cur = (right + left) /2;
    cmp = strcmp(hst->data[cur],hash);
    if (cmp == 0)
     {
      found = cur;
      break;
     }
    else if (cmp < 0)
     {
      left = cur+1;
     }
    else
     {
      right = cur-1;
     }
   }

  if (found < 0)
   {
    if (loc != NULL)
     {
      if (hst->n_data == 0) *loc = 0;
      else *loc = left;
     }
   }
  else
   {
    if (loc != NULL) *loc = found;
   }

  return found;
}

/*
 * job_registry_free_hash_store
 *
 * Free hash storage.
 *
 * @param hst Pointer to a hash store handle. 
 *
 */

void
job_registry_free_hash_store(job_registry_hash_store *hst)
{
  int i;

  if (hst == NULL) return;

  if (hst->data == NULL)
   {
    hst->n_data = 0;
    return;
   }

  for (i=0; i < hst->n_data; i++)
   {
    if ((hst->data[i]) != NULL) free(hst->data[i]);
   }
  free(hst->data);
  hst->data = NULL;
  hst->n_data = 0;
}

/*
 * job_registry_purge_subject_hash_list
 *
 * Purge the subject list in the job registry, leaving only the entries
 * listed in the supplied hash store.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param hst Pointer to a job_registry_hash_store containing an
 *            ordered array of the entries to be saved.
 *
 * @return 0 in case of success, < 0 in case of failure 
 *                               (errno is set accordingly).
 */

int
job_registry_purge_subject_hash_list(const job_registry_handle *rha,
                                     const job_registry_hash_store *hst)
{
  FILE *rfd, *wfd;
  char subline[JOB_REGISTRY_MAX_SUBJECTLIST_LINE];
  int retcod;
  char *en;
  char *templist = NULL;

  if (rha == NULL || hst == NULL) return -1;

  rfd = fopen(rha->subjectlist, "r");
  if (rfd == NULL) return -1;

  /* Create purged registry file that will be rotated in place */
  /* of the current registry. */
  templist = jobregistry_construct_path("%s/%s.new_subjectlist.%d", 
                                        rha->path, getpid());
  if (templist == NULL) 
   {
    fclose(rfd);
    return -1;
   }

  if ((wfd = fopen(templist, "w")) == NULL)
   {
    fclose(rfd);
    free(templist);
    return -1;
   }

  if (job_registry_rdlock(rha, rfd) < 0)
   {
    fclose(rfd);
    fclose(wfd);
    unlink(templist);
    free(templist);
    return -1;
   }

  while (!feof(rfd))
   {
    if (fgets(subline, sizeof(subline), rfd) != NULL)
     {
      en = strchr(subline, ' ');
      if (en == NULL) continue; /* Bogus entry ?! */
      *en = '\000';
      if (job_registry_lookup_hash(hst, subline, NULL) >= 0)
       {
        /* Entry found. Write it out. */
        *en = ' ';
        if (fputs(subline, wfd) < 0)
         {
          fclose(rfd);
          fclose(wfd);
          unlink(templist);
          free(templist);
          return -1;
         }
       }
     }
   }

  fclose(rfd);
  fclose(wfd);

  /* Rotate new subjectlist in place */
  retcod = rename(templist, rha->subjectlist);
  if (retcod < 0) unlink(templist);
  free(templist);

  return retcod;
}

/*
 * job_registry_check_index_key_uniqueness
 *
 * Check whether in the current index (if any) there are any duplicated
 * entries. 
 * Will optionally return the ID of the first duplicated entry.
 *
 * @param rha Pointer to a job registry handle returned by job_registry_init.
 * @param first_duplicate_id If non-NULL, will be set to
 *                           a pointer to the first duplicated ID that
 *                           was found.
 * @return 0 if no duplicates are found, < 0 if they are found.
 */

int job_registry_check_index_key_uniqueness(const job_registry_handle *rha,
                                            char **first_duplicate_id)
{
  int cur;

  for (cur=1; cur < rha->n_entries; cur++)
   {
    if (strcmp(rha->entries[cur-1].id, rha->entries[cur].id) == 0)
     {
      if (first_duplicate_id != NULL) 
        *first_duplicate_id=rha->entries[cur-1].id;
      return JOB_REGISTRY_FAIL;
     }
   }

  return JOB_REGISTRY_SUCCESS; 
}
