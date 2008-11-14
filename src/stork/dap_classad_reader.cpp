/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"
#include "dap_error.h"

//--------------------------------------------------------------------
int getValue(classad::ClassAd *currentAd, char *attr, char *attrval)
{
  char tempattrval[MAXSTR];
  std::string adbuffer = "";
  classad::ClassAdUnParser unparser;
  classad::ExprTree *attrexpr = NULL;
  char unstripped[MAXSTR];

  if (!(attrexpr = currentAd->Lookup(attr)) ){
    //    printf("%s not found!\n", attr);
    strncpy(attrval, "", MAXSTR);
    return DAP_ERROR;
  }

  unparser.Unparse(adbuffer,attrexpr);
  strncpy(attrval, adbuffer.c_str(), MAXSTR);

  strncpy(unstripped, attrval, MAXSTR);

  //look for $ macro functions
  strncpy(tempattrval, strip_str(unstripped), MAXSTR);  
  if (tempattrval[0] == '$'){
    char functionname[MAXSTR];
    
    strcpy(functionname, strtok(tempattrval, "(") );

    //-------- if the function is "read_from_file -----------
    if (!strcmp(functionname, "$read_from_file")){
      char filename[MAXSTR], attrname[MAXSTR];
      char *p;
      
      p = strtok(NULL,",");
      if (p != NULL){
	strcpy(filename, strip_str(p));                 
      }
      else{
	strcpy(filename, "");
      }
      

      p = strtok(NULL,")");
      if (p != NULL){
	strcpy(attrname, strip_str(p));              
      }
      else{
	strcpy(attrname, "");
      }

      //read the argumant from file
      classad::ClassAd *newAd = 0;
      classad::ClassAdParser parser;
      FILE *f;
      
      f = safe_fopen_wrapper(filename, "r");
      if (f == NULL){
	dprintf(D_ALWAYS, "error in opening file: %s\n", filename);
	return DAP_ERROR;
      }
      
      newAd = parser.ParseClassAd(f);

      getValue(newAd, attrname, attrval);
      strcpy(attrval, strip_str(attrval));
    }// ------------ end of read_from_file ---------------------

  }
  
  return DAP_SUCCESS; 
}



//---------------------------------------
// USED TO READ FROM THE HISTORY LOGS
//----------------------------------------
ClassAd_Reader::ClassAd_Reader(const char *fname)
{

  currentAd = NULL;
  
  adfile = safe_fopen_wrapper(fname,"r");
  //printf("classad:open:%d\n",++num_files);
  if (adfile == NULL) adfile = safe_fopen_wrapper(fname,"w");
  if (adfile == NULL) {
    dprintf(D_ALWAYS,"cannot open file:%s\n",fname);
  exit(1);
  }
}
//----------------------------------------  
ClassAd_Reader::~ClassAd_Reader()
{
  if (currentAd != NULL){ delete currentAd;}

  fclose(adfile);
}
//----------------------------------------  
void
ClassAd_Reader::reset()
{
  fseek(adfile,0,SEEK_SET);
}

//----------------------------------------
int
ClassAd_Reader::readAd()
{
  int i;
  char c;
  std::string adstr="";
  classad::ClassAdParser parser;
  int leftparan = 0;

  i =0;
  while (1){
    c = fgetc(adfile);
    if (c == ']'){ 
      leftparan --; 
      if (leftparan == 0) break;
    }
    if (c == '[') leftparan ++; 

    if (c == EOF) return 0;
    adstr += c;
    i++;
  }
  adstr += c;

  //dprintf(D_ALWAYS,"ClassAd:%s\n",adstr.c_str());
  
  if (currentAd != NULL){ delete currentAd; currentAd = NULL; }


  currentAd = parser.ParseClassAd(adstr);

  if (currentAd == NULL) {
    dprintf(D_ALWAYS,"Invalid classAd!\n");
    dprintf(D_ALWAYS,"ClassAd:%s\n",adstr.c_str());
    return 0;
  }
  else return 1;

}
//----------------------------------------
int
ClassAd_Reader::getValue(char *attr, char *attrval)
{
  std::string adbuffer = "";
  classad::ClassAdUnParser unparser;
  classad::ExprTree *attrexpr = NULL;

  if (!(attrexpr = currentAd->Lookup(attr)) ){
    //    printf("%s not found!\n", attr);
    return DAP_ERROR;
  }

  unparser.Unparse(adbuffer,attrexpr);
  //  strcpy(attrval,strip_str(adbuffer.c_str()));
  strncpy(attrval, adbuffer.c_str(), MAXSTR);

  //  printf("VALUE => %s\n",adbuffer.c_str());

  //  if (attrexpr != NULL) delete attrexpr;
  return DAP_SUCCESS;
}

/*----------------------------------------
 * Get attrval1
 * where attr2 = attrval2
 *----------------------------------------*/
int 
ClassAd_Reader::queryAttr(char *attr1, char* attrval1, char *attr2,char *attrval2 )
{
  char tempval[MAXSTR];
  std::string adbuffer = "";  
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;

  while (1){
    if (!readAd()){
      //if (attrexpr != NULL) delete attrexpr;
      return 0;
    }
    getValue(attr2,tempval);

    if (!strcmp(attrval2, tempval)){
      attrexpr = currentAd->Lookup(attr1) ;
      unparser.Unparse(adbuffer,attrexpr);
      strncpy(attrval1,adbuffer.c_str(), MAXSTR);

      //      if (attrexpr != NULL) delete attrexpr;
      return 1;
    }
  }// while 

}
/*----------------------------------------
 * return the current classad
 *----------------------------------------*/
classad::ClassAd*
ClassAd_Reader::returncurrentAd()
{
  return currentAd;
}

/*----------------------------------------
 * return the classad 
 * where keyattr = keyval && attr = attrval
 *----------------------------------------*/
int 
ClassAd_Reader::lookFor(char *keyattr, const std::string keyval, char *attr,char *attrval )
{
  char tempval[MAXSTR];
  std::string adbuffer = "";  
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;
  
  reset();

  //printf("looking for:%s\n",keyval.c_str());
  while (1){
    if (!readAd()) { 
      //      if (attrexpr != NULL) delete attrexpr;
      return 0;
    }

    getValue(attr,tempval);

    if (!strcmp(attrval, tempval)){
      attrexpr = currentAd->Lookup(keyattr) ;
      unparser.Unparse(adbuffer,attrexpr);

      //      printf("Comparing:%s\n",adbuffer.c_str());
      if (keyval == adbuffer){
	//        if (attrexpr != NULL) delete attrexpr;
	return 1;
      }
      adbuffer="";
    }
  }// while 

  //  if (attrexpr != NULL) delete attrexpr;
  return 0;
}

/*----------------------------------------
 * return value of qattr to qval from  first matching classad 
 * where attr = attrval
 *----------------------------------------*/
int 
ClassAd_Reader::returnFirst(const std::string qattr,char *qval, char *attr,char *attrval )
{
  char tempval[MAXSTR];
  std::string adbuffer = "";  
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;
  
  reset();

  while (1){
    if (!readAd()) {
      //      if (attrexpr != NULL) delete attrexpr;
      return 0;
    }

    getValue(attr,tempval);
    
    if (!strcmp(attrval, tempval)){
      attrexpr = currentAd->Lookup(qattr) ;
      unparser.Unparse(adbuffer,attrexpr);
      strncpy(qval,adbuffer.c_str(), MAXSTR);

      //      if (attrexpr != NULL) delete attrexpr;
      return 1;
    }
  }// while 

}

int get_subAdval(classad::ClassAd *parentAd, char* subAdname, char *attr, char* attrval)
{
  std::string adbuffer = "";
  classad::ClassAdUnParser unparser;
  classad::ClassAdParser parser;
  classad::ExprTree *attrexpr = NULL;
  char temp[MAXSTR];
  int len;
  
  //parse parent classAd -----------
  if (!(attrexpr = parentAd->Lookup(subAdname)) ){
    dprintf(D_ALWAYS, "%s not found!\n", subAdname);
    return 0;
  }

  unparser.Unparse(adbuffer,attrexpr);
  //printf("SUB_AD VALUE => %s\n",adbuffer.c_str());
  //  if (attrexpr != NULL) delete attrexpr;  
  

  //parse sub classAd ---------
  classad::ClassAd *subAd = NULL;

  subAd = parser.ParseClassAd(adbuffer);
  adbuffer = "";
  if (!(attrexpr = subAd->Lookup(attr)) ){
    dprintf(D_ALWAYS, "%s not found!\n", attr);
    return 0;
  }

  unparser.Unparse(adbuffer,attrexpr);
  //  printf("ATTR VALUE => %s\n",adbuffer.c_str());

  strncpy(temp,adbuffer.c_str(), MAXSTR);

  if (temp[0] == '"'){ // contains quotes, so remove them
    len = strlen(temp);
    for (int i=1;i<len;i++)
      attrval[i-1] = temp[i];
    attrval[len-2] = '\0';
  }
  else
    strncpy(attrval,temp, MAXSTR);

  //if (attrexpr != NULL) delete attrexpr;
  if (subAd != NULL) delete subAd;

  return 1;
}






