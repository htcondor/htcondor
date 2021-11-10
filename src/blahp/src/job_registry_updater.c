/*
 *  File :     job_registry_updater.c
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  13-Jul-2011 Original release
 *  19-Jul-2011 Added transfer of full proxy subject and path.
 *
 *  Description:
 *    Protocol to distribute network updates to the BLAH registry.
 *    This was added to implement a set of highly available BLAH
 *    servers with the ability to operate on the same job set.
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
#include <string.h>

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h> 

#include "job_registry_updater.h"

/*
 * job_registry_updater_parse_address
 *
 * Parse an address string, in the format "interface|host:port",
 * and hand it over to the resolver .
 *
 * @param addsrt Address strings in the format "interface|host:port".
 *        All parts of the address are optional: defaults will be used.
 * @param ai_ans Pointer to the obtained getaddrinfo reply.
 *        If this function succeeds, it needs to be freed via freeaddrinfo.
 * @param Value pointed to by ifindex is set to the index of the specified 
 *        interface (if present). Will be set to 0 otherwise.
 *
 * @return Less than zero on errors. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_updater_parse_address(const char *addstr, struct addrinfo **ai_ans,
                                   unsigned int *ifindex)
{
  char *pipe_loc;
  char *addr_loc;
  char *port_loc;
  char *locaddr;
  struct addrinfo ai_req;
  int retcod;

  if (ai_ans == NULL || ifindex == NULL)
   {
    errno = EINVAL;
    return JOB_REGISTRY_FAIL;
   }

  if ((locaddr = strdup(addstr)) == NULL)
   {
    errno = ENOMEM;
    return JOB_REGISTRY_MALLOC_FAIL;
   }

  /* Look for leading interface specification */
  if ((pipe_loc = strchr(locaddr,'|')) != NULL)
   {
    addr_loc = pipe_loc+1; 
    *pipe_loc = '\000';
    *ifindex = if_nametoindex(locaddr);
   }
  else
   {
    addr_loc = locaddr;
    *ifindex = 0;
   }

  if ((port_loc=strrchr(addr_loc, ':')) != NULL)
   {
    *port_loc = '\000';
    port_loc++;
   }
  else port_loc = _job_registry_updater_h_DEFAULT_PORT;

  if (strlen(addr_loc) <= 0) addr_loc = _job_registry_updater_h_DEFAULT_MCAST_ADDR;

  ai_req.ai_flags = 0;
  ai_req.ai_family = PF_UNSPEC;
  ai_req.ai_socktype = SOCK_DGRAM;
  ai_req.ai_protocol = 0; /* Any protocol is OK */

  retcod = getaddrinfo(addr_loc, port_loc, &ai_req, ai_ans);
  return retcod;
}

/*
 * job_registry_updater_is_multicast
 *
 * Determine whether the address pointed to by the argument
 * is an IPv4 or IPv6 multicast address.
 *
 * @param cur_ans Address structure as returned by getaddrinfo.
 *
 * @return Boolean valued integer. Non-zero if address is multicast.
 */

int
job_registry_updater_is_multicast(const struct addrinfo *cur_ans)
{
  int is_multicast = FALSE;
  switch(cur_ans->ai_family) 
   {
    case AF_INET:
      /* IPV4 case */
      if (((((struct sockaddr_in *)
             (cur_ans->ai_addr))->sin_addr.s_addr) & 0xe0)
                                                  == 0xe0)
      is_multicast = TRUE;
      break;
    case AF_INET6:
      /* IPV6 case */
      if (IN6_IS_ADDR_MULTICAST(((struct sockaddr_in6 *)
                      (cur_ans->ai_addr))->sin6_addr.s6_addr))
      is_multicast = TRUE;
      break;
    default:
      break;
   }
  return is_multicast;
}

/*
 * job_registry_updater_setup_sender
 *
 * Bind a socket for every network destination listed.
 * IPv4, IPv6, unicast and multicast are supported.
 *
 * @param destinations Array of strings in the format "interface|host:port".
 *        All parts of the address are optional: defaults will be used.
 * @param n_destinations Number of valid entries in 'destinations'.
 * @param ttl Time-to-live to be set for multicast endpoints.
 * @param endpoints Pointer to a linked list of endpoint structures. This
 *        will be appended to with the successfully created endpoints.
 *
 * @return Number of sender endpoints that could be successfully set up.
 *         Less than zero on errors that prevent the creation of any endpoint. 
 *         See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_updater_setup_sender(char **destinations, int n_destinations,
                                  unsigned char ttl,
                                  job_registry_updater_endpoint **endpoints)
{
  int i;
  struct addrinfo *ai_ans, *cur_ans;
  unsigned int ifindex;
  int pretcod,tretcod,retsetsock;
  int tfd;
  job_registry_updater_endpoint *cur, *last=NULL;
  job_registry_updater_endpoint *new_endpoint;
  int n_added = 0;
  int is_multicast;
  struct ip_mreqn mreq4;

  if (endpoints == NULL)
   {
    errno = EINVAL;
    return JOB_REGISTRY_FAIL;
   }

  if (ttl == 0) ttl = _job_registry_updater_h_DEFAULT_TTL;

  for (cur = *endpoints; cur!=NULL; cur=cur->next)
   {
    last = cur;
   }

  for (i=0; i<n_destinations; ++i)
   {
    if ((pretcod = job_registry_updater_parse_address(destinations[i], &ai_ans, &ifindex)) >= 0)
     {
      /* Look for a workable address */
      for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next)
       {
        tfd = socket(cur_ans->ai_family, cur_ans->ai_socktype, cur_ans->ai_protocol);
        if (tfd < 0)
         {
          if (n_destinations == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_SOCKET_FAIL;
           }
          else continue; 
         }
        is_multicast = job_registry_updater_is_multicast(cur_ans);
        if (is_multicast)
         {
          switch(cur_ans->ai_family)
           {
            case AF_INET:
              /* IPV4 case */
              memcpy(&mreq4.imr_multiaddr,
                     &(((struct sockaddr_in*)(cur_ans->ai_addr))->sin_addr),
                     sizeof(struct in_addr));
              mreq4.imr_address.s_addr = htonl(INADDR_ANY);
              mreq4.imr_ifindex = ifindex;
   
              retsetsock= setsockopt(tfd, IPPROTO_IP, IP_MULTICAST_IF,
                                     &mreq4, sizeof(mreq4));
              if (retsetsock < 0)
               {
                close(tfd);
                if (n_destinations == 1)
                 {
                  freeaddrinfo(ai_ans);
                  return JOB_REGISTRY_MCAST_FAIL;
                 }
                else continue; 
               }
              break;
            case AF_INET6:
              /* IPV6 case */
              retsetsock = setsockopt(tfd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                      &ifindex, sizeof(ifindex));
              if (retsetsock < 0)
               {
                close(tfd);
                if (n_destinations == 1)
                 {
                  freeaddrinfo(ai_ans);
                  return JOB_REGISTRY_MCAST_FAIL;
                 }
                else continue; 
               }
              break;
            default:
              /* Don't know what to do with non IPv4/IPv6 addresses. */
              continue;
           }
         }
        /* Get new endpoint */
        new_endpoint = (job_registry_updater_endpoint *)malloc(sizeof(job_registry_updater_endpoint));
        if (new_endpoint == NULL)
         {
          close(tfd);
          if (n_destinations == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_MALLOC_FAIL;
           }
          else continue;
         }
        new_endpoint->fd = tfd;
        new_endpoint->addr_family = cur_ans->ai_family;
        new_endpoint->is_multicast = is_multicast;
        new_endpoint->next = NULL;
        if (job_registry_updater_set_ttl(new_endpoint, ttl) < 0)
         {
          close(new_endpoint->fd);
          free(new_endpoint);
          if (n_destinations == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_TTL_FAIL;
           }
          else continue;
         }
        if (connect(new_endpoint->fd, cur_ans->ai_addr, cur_ans->ai_addrlen) < 0)
         {
          close(new_endpoint->fd);
          free(new_endpoint);
          if (n_destinations == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_CONNECT_FAIL;
           }
          else continue;
         }
        
        /* All is well. Add endpoint to list. */
        if (last == NULL)
         {
          *endpoints = new_endpoint;
          last = new_endpoint;
         }
        else last->next = new_endpoint;
        n_added++;
       }
      freeaddrinfo(ai_ans);
     } 
    else if (n_destinations == 1)
     {
      return pretcod;
     }
   }
  return n_added;
}

/*
 * job_registry_updater_setup_receiver
 *
 * Bind a socket for every network source listed.
 * IPv4, IPv6, unicast and multicast are supported.
 *
 * @param sources Array of strings in the format "interface|host:port".
 *        All parts of the address are optional: defaults will be used.
 * @param n_sources Number of valid entries in 'sources'.
 * @param endpoints Pointer to a linked list of endpoint structures. This
 *        will be appended to with the successfully created endpoints.
 *
 * @return Number of sender endpoints that could be successfully set up.
 *         Less than zero on errors that prevent the creation of any endpoint. 
 *         See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_updater_setup_receiver(char **sources, int n_sources,
                                    job_registry_updater_endpoint **endpoints)
{
  int i;
  struct addrinfo *ai_ans, *cur_ans;
  unsigned int ifindex;
  int pretcod,retsetsock;
  int tfd;
  job_registry_updater_endpoint *cur, *last=NULL;
  job_registry_updater_endpoint *new_endpoint;
  int n_added = 0;
  int is_multicast;
  struct ip_mreqn mreq4;
  struct ipv6_mreq mreq6;

  if (endpoints == NULL)
   {
    errno = EINVAL;
    return JOB_REGISTRY_FAIL;
   }

  for (cur = *endpoints; cur!=NULL; cur=cur->next)
   {
    last = cur;
   }

  for (i=0; i<n_sources; ++i)
   {
    if ((pretcod = job_registry_updater_parse_address(sources[i], &ai_ans, &ifindex)) >= 0)
     {
      /* Look for a workable address */
      for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next)
       {
        tfd = socket(cur_ans->ai_family, cur_ans->ai_socktype, cur_ans->ai_protocol);
        if (tfd < 0)
         {
          if (n_sources == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_SOCKET_FAIL;
           }
          else continue; 
         }
        is_multicast = job_registry_updater_is_multicast(cur_ans);
        if (is_multicast)
         {
          switch(cur_ans->ai_family)
           {
            case AF_INET:
              /* IPV4 case */
              memcpy(&mreq4.imr_multiaddr,
                     &(((struct sockaddr_in*)(cur_ans->ai_addr))->sin_addr),
                     sizeof(struct in_addr));
              mreq4.imr_address.s_addr = htonl(INADDR_ANY);
              mreq4.imr_ifindex = ifindex;
   
              retsetsock = setsockopt(tfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                     &mreq4, sizeof(mreq4));
              if (retsetsock < 0)
               {
                close(tfd);
                if (n_sources == 1)
                 {
                  freeaddrinfo(ai_ans);
                  return JOB_REGISTRY_MCAST_FAIL;
                 }
                else continue; 
               }
              /* Bind to any address. */
              ((struct sockaddr_in*)(cur_ans->ai_addr))->sin_addr.s_addr = INADDR_ANY;
              break;
            case AF_INET6:
              /* IPV6 case */
              memcpy(&mreq6.ipv6mr_multiaddr,
                     &(((struct sockaddr_in6*)(cur_ans->ai_addr))->sin6_addr),
                     sizeof(struct in6_addr));
              mreq6.ipv6mr_interface = ifindex;
 
              retsetsock= setsockopt(tfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                                     &mreq6, sizeof(mreq6));
              if (retsetsock < 0)
               {
                close(tfd);
                if (n_sources == 1)
                 {
                  freeaddrinfo(ai_ans);
                  return JOB_REGISTRY_MCAST_FAIL;
                 }
                else continue; 
               }
              /* Bind to any address. */
              ((struct sockaddr_in6*)(cur_ans->ai_addr))->sin6_addr = in6addr_any;
              break;
            default:
              /* Don't know what to do with non IPv4/IPv6 addresses. */
              continue;
           }
         }
        if (bind(tfd, cur_ans->ai_addr, cur_ans->ai_addrlen) < 0)
         {
          close(tfd);
          if (n_sources == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_BIND_FAIL;
           }
          else continue;
         }
        /* All is well. Add a new endpoint to the list. */
        new_endpoint = (job_registry_updater_endpoint *)malloc(sizeof(job_registry_updater_endpoint));
        if (new_endpoint == NULL)
         {
          close(tfd);
          if (n_sources == 1)
           {
            freeaddrinfo(ai_ans);
            return JOB_REGISTRY_MALLOC_FAIL;
           }
          else continue;
         }
        new_endpoint->fd = tfd;
        new_endpoint->addr_family = cur_ans->ai_family;
        new_endpoint->is_multicast = is_multicast;
        new_endpoint->next = NULL;
        if (last == NULL)
         {
          *endpoints = new_endpoint;
          last = new_endpoint;
         }
        else last->next = new_endpoint;
        n_added++;
       }
      freeaddrinfo(ai_ans);
     }
    else if (n_sources == 1)
     {
      return pretcod;
     }
   }
  return n_added;
}

/*
 * job_registry_updater_set_ttl
 *
 * Set the multicast TTL of the specified endpoint, if applicable,
 * to the supplied value.
 *
 * @param endpoint Endpoint structure. The file descriptor must point to an
 *        open file, and the is_multicast and addr_family fields must be correctly set.
 *
 * @return Less than zero on errors. See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_updater_set_ttl(job_registry_updater_endpoint *endp,
                             unsigned char ttl)
{
  int retsetsock;
  int hops; 

  if (endp == NULL)
   {
    errno = EINVAL;
    return JOB_REGISTRY_FAIL;
   }

  if (!(endp->is_multicast)) return JOB_REGISTRY_UNCHANGED;
  if (ttl == 0) return JOB_REGISTRY_UNCHANGED;

  switch(endp->addr_family)
   {
    case AF_INET:
      retsetsock = setsockopt(endp->fd, IPPROTO_IP, IP_MULTICAST_TTL,
                                &ttl, sizeof(ttl));
      break;
    case AF_INET6:
      hops = ttl;
      retsetsock = setsockopt(endp->fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                              &hops, sizeof(hops));
      break;
    default:
      return JOB_REGISTRY_UNCHANGED;
   }
  return retsetsock;
}

/*
 * job_registry_updater_free_endpoints
 *
 * Free the endpoints linked list filled by either
 * job_registry_updater_setup_sender or job_registry_updater_setup_receiver.
 *  
 * @param head Pointer to the first endpoint list entry.
 */

void
job_registry_updater_free_endpoints(job_registry_updater_endpoint *head)
{
  job_registry_updater_endpoint *next, *cur = head;

  while (cur != NULL)
   {
    if ((cur->fd) >= 0) close(cur->fd);
    next = cur->next;
    free(cur);
    cur = next;
   }
}

/*
 * job_registry_send_update
 *
 * Send an updated entry to a list of endpoints.
 *
 * @param endpoints Head of a linked list of endpoint structures. 
 * @param entry Job registry entry to be sent. 
 * @param proxy_subject Optional proxy subject to be sent with the packet.
 * @param proxy_path Optional proxy (shared) path to be sent with the packet.
 *
 * @return Number of sender endpoints that could be successfully set up.
 *         Less than zero on errors that prevent the creation of any endpoint. 
 *         See job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_send_update(const job_registry_updater_endpoint *endpoints,
                         const job_registry_entry *entry,
                         const char *proxy_subject,
                         const char *proxy_path)
{
  const job_registry_updater_endpoint *cur;
  int n_success = 0;
  int retcod;
  const int max_sndstr = 2;
  int n_sndstr;
  const char *sndstr[max_sndstr];
  unsigned short sndstrlen;
  ssize_t strptr;
  unsigned char *packet;
  ssize_t plen = 0;

  sndstr[0] = proxy_subject;
  sndstr[1] = proxy_path;

  plen = sizeof(job_registry_entry);
  for (n_sndstr = 0; n_sndstr < max_sndstr; n_sndstr++)
   {
    if (sndstr[n_sndstr] != NULL)
     {
      plen += sizeof(sndstrlen) + strlen(sndstr[n_sndstr]) + 1;
     }
   }

  /* TODO: add check for maximum plen */

  packet = (unsigned char *)malloc(plen);
  if (packet == NULL)
   {
    errno = ENOMEM;
    return JOB_REGISTRY_MALLOC_FAIL;
   }
  memcpy(packet, entry,  sizeof(job_registry_entry));

  /* Invalidate fields that have only local meaning */
  ((job_registry_entry *)packet)->proxy_link[0] = '\000';
  ((job_registry_entry *)packet)->subject_hash[0] = '\000';

  strptr = sizeof(job_registry_entry);

  /* Receive additional strings tacked to the entry message */
  /* in the format */
  /* |exclusive string length (unsigned short)|null-terminated string|*/
  for (n_sndstr = 0; n_sndstr < max_sndstr; n_sndstr++)
   {
    if (sndstr[n_sndstr] != NULL)
     {
      sndstrlen = strlen(sndstr[n_sndstr]);
      *(unsigned short *)(packet + strptr) = sndstrlen;
      strptr += sizeof(sndstrlen);
      strcpy(packet + strptr, sndstr[n_sndstr]);
      strptr += sndstrlen + 1;
     }
   }

  for (cur = endpoints; cur != NULL; cur = cur->next)
   {
    if ((retcod = send(cur->fd, packet, plen, 0)) == plen)
     {
      n_success++;
     }
   }

  return n_success;
}

/*
 * job_registry_updater_get_pollfd
 *
 * Prepare a pollset from a set of endpoints for subsequent use
 * in job_registry_receive_update (or poll).
 *
 * @param endpoints Head of a linked list of endpoint structures. 
 * @param pollset Pointer to a pollfd structure to be filled by this call. 
 *
 * @return Number of entries added into pollset. If result is > 0 pollset
 *         needs to be freed after use.
 *         Less than zero on errors - see job_registry.h for error codes.
 *         errno is also set in case of error.
 */

int
job_registry_updater_get_pollfd(job_registry_updater_endpoint *endpoints,
                                struct pollfd **pollset)
{
  job_registry_updater_endpoint *cur;
  struct pollfd *new, *ret = NULL;
  int n_fd = 0;

  if (pollset == NULL) 
   {
    errno = EINVAL;
    return JOB_REGISTRY_FAIL;
   }

  *pollset = NULL;

  for (cur = endpoints; cur != NULL; cur = cur->next)
   {
    new = (struct pollfd *)realloc(ret, (n_fd + 1)*sizeof(struct pollfd)); 
    if (new == NULL)
     {
      if (ret != NULL) free(ret);
      errno = ENOMEM;
      return JOB_REGISTRY_MALLOC_FAIL;
     }
    ret = new;
    ret[n_fd].fd = cur->fd;
    ret[n_fd].events = POLLIN;
    ret[n_fd].revents = 0;
    n_fd++;
   }

  *pollset = ret;
  return n_fd;
}

/*
 * job_registry_receive_update
 *
 * Poll a list of endpoints and receive one pending update entry.
 *
 * @param pollset Set of files that should be polled for. Can be
 *        obtained from a list of endpoints via job_registry_updater_get_pollfd
 * @param nfds Number of valid file descriptors in pollset.
 * @param timeout_ms Timeout (in milliseconds) of poll call.
 * @param proxy_subject Pointer that can be set to a strdupped value
 *                      of the received proxy subject, if present.
 * @param proxy_path Pointer that can be set to a strdupped value
 *                   of the received full (shared) proxy path, if present.
 *
 * @return Received entry (dynamically allocated: need to be free'd),
 *         or NULL if a timeout is encountered. 
 */

job_registry_entry *
    job_registry_receive_update(struct pollfd *pollset, nfds_t nfds,
                                int timeout_ms,
                                char **proxy_subject, char **proxy_path)
{
  int retpoll;
  ssize_t retrecv;
  ssize_t strptr;
  int chosen;
  int n_ready;
  int i;
  unsigned char buffer[4096];
  const int max_retstr = 2;
  int n_retstr = 0;
  char **retstr[max_retstr];
  unsigned short tlen;
  job_registry_entry *test, *retval = NULL;
  
  retstr[0] = proxy_subject;
  retstr[1] = proxy_path;

  while (1) /* Will exit on timeout or when an event is received */
   {
    retpoll = poll(pollset, nfds, timeout_ms);
    if (retpoll <= 0) return NULL;

    /* Pick an event from a randomly chosen ready FD */
    chosen = rand()%retpoll;
    n_ready = 0;
    for (i=0; i<nfds; ++i)
     {
      if (pollset[i].revents != 0)
       {
        if (n_ready == chosen)
         {
          retrecv = recv(pollset[i].fd, buffer, sizeof(buffer), 0); 
          if (retrecv >= sizeof(job_registry_entry))
           {
            test = (job_registry_entry *)buffer;
            if ( ((test->magic_start) == JOB_REGISTRY_MAGIC_START) &&
                 ((test->magic_end)   == JOB_REGISTRY_MAGIC_END) )

             {
              retval = (job_registry_entry *)malloc(sizeof(job_registry_entry));
              if (retval != NULL)
               {
                memcpy(retval, test, sizeof(job_registry_entry));
               }
             }

            /* Receive additional strings tacked to the entry message */
            /* in the format */
            /* |exclusive string length (unsigned short)|null-terminated string|*/
            for(strptr = sizeof(job_registry_entry);
                (strptr < retrecv) && (n_retstr < max_retstr); n_retstr++)
             {
              tlen = *(unsigned short *)(buffer + strptr);
              if (*(buffer + strptr + sizeof(unsigned short) + tlen) == '\000')
               {
                if (retstr[n_retstr] != NULL) 
                  *(retstr[n_retstr]) = strdup(buffer + strptr + sizeof(unsigned short));
               }
              strptr += tlen + sizeof(unsigned short) + 1;
             }
            /* TODO: May want to do some data conversion here */
           }
          break;
         }
        n_ready++;
       }
     }
    if (retval != NULL) break;
   }
  return retval;
}

