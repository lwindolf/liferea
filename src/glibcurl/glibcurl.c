/* $Id: glibcurl.c,v 1.14 2004/12/05 16:15:12 atterer Exp $ -*- C -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  All rights reserved.

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
  USE OR OTHER DEALINGS IN THE SOFTWARE.

  Except as contained in this notice, the name of a copyright holder shall
  not be used in advertising or otherwise to promote the sale, use or other
  dealings in this Software without prior written authorization of the
  copyright holder.

*/

#include <glib.h>
#include <glibcurl.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* #define D(_args) fprintf _args; */
#define D(_args)

/* #if 1 */
#ifdef G_OS_WIN32
/*______________________________________________________________________*/

/* Timeout for the fds passed to glib's poll() call, in millisecs.
   curl_multi_fdset(3) says we should call curl_multi_perform() at regular
   intervals. */
#define GLIBCURL_TIMEOUT 500

/* A structure which "derives" (in glib speak) from GSource */
typedef struct CurlGSource_ {
  GSource source; /* First: The type we're deriving from */

  CURLM* multiHandle;
  GThread* selectThread;
  GCond* cond; /* To signal selectThread => main thread: call perform() */
  GMutex* mutex; /* Not held by selectThread whenever it is waiting */

  gboolean callPerform; /* TRUE => Call curl_multi_perform() Real Soon */
  gint gtkBlockAndWait;
  gboolean selectRunning; /* FALSE => selectThread terminates */

  /* For data returned by curl_multi_fdset */
  fd_set fdRead;
  fd_set fdWrite;
  fd_set fdExc;
  int fdMax;

} CurlGSource;

/* Global state: Our CurlGSource object */
static CurlGSource* curlSrc = 0;

/* The "methods" of CurlGSource */
static gboolean prepare(GSource* source, gint* timeout);
static gboolean check(GSource* source);
static gboolean dispatch(GSource* source, GSourceFunc callback,
                         gpointer user_data);
static void finalize(GSource* source);

static GSourceFuncs curlFuncs = {
  &prepare, &check, &dispatch, &finalize, 0, 0
};
/*______________________________________________________________________*/

void glibcurl_init() {
  /* Create source object for curl file descriptors, and hook it into the
     default main context. */
  GSource* src = g_source_new(&curlFuncs, sizeof(CurlGSource));
  curlSrc = (CurlGSource*)src;
  g_source_attach(&curlSrc->source, NULL);

  if (!g_thread_supported()) g_thread_init(NULL);

  /* Init rest of our data */
  curlSrc->callPerform = 0;
  curlSrc->selectThread = 0;
  curlSrc->cond = g_cond_new();
  curlSrc->mutex = g_mutex_new();
  curlSrc->gtkBlockAndWait = 0;

  /* Init libcurl */
  curl_global_init(CURL_GLOBAL_ALL);
  curlSrc->multiHandle = curl_multi_init();
}
/*______________________________________________________________________*/

void glibcurl_cleanup() {
  D((stderr, "glibcurl_cleanup\n"));
  /* You must call curl_multi_remove_handle() and curl_easy_cleanup() for all
     requests before calling this. */
/*   assert(curlSrc->callPerform == 0); */

  /* All easy handles must be finished */

  /* Lock before accessing selectRunning/selectThread */
  g_mutex_lock(curlSrc->mutex);
  curlSrc->selectRunning = FALSE;
  while (curlSrc->selectThread != NULL) {
    g_mutex_unlock(curlSrc->mutex);
    g_thread_yield();
    g_cond_signal(curlSrc->cond); /* Make the select thread shut down */
    g_thread_yield();
    g_mutex_lock(curlSrc->mutex); /* Wait until it has shut down */
  }
  g_mutex_unlock(curlSrc->mutex);

  assert(curlSrc->selectThread == NULL);

  g_cond_free(curlSrc->cond);
  g_mutex_free(curlSrc->mutex);

  curl_multi_cleanup(curlSrc->multiHandle);
  curlSrc->multiHandle = 0;
  curl_global_cleanup();

  g_source_unref(&curlSrc->source);
  curlSrc = 0;
}
/*______________________________________________________________________*/

CURLM* glibcurl_handle() {
  return curlSrc->multiHandle;
}
/*______________________________________________________________________*/

CURLMcode glibcurl_add(CURL *easy_handle) {
  assert(curlSrc != 0);
  assert(curlSrc->multiHandle != 0);
  glibcurl_start();
  return curl_multi_add_handle(curlSrc->multiHandle, easy_handle);
}
/*______________________________________________________________________*/

CURLMcode glibcurl_remove(CURL *easy_handle) {
  D((stderr, "glibcurl_remove %p\n", easy_handle));
  assert(curlSrc != 0);
  assert(curlSrc->multiHandle != 0);
  return curl_multi_remove_handle(curlSrc->multiHandle, easy_handle);
}
/*______________________________________________________________________*/

/* Call this whenever you have added a request using
   curl_multi_add_handle(). */
void glibcurl_start() {
  D((stderr, "glibcurl_start\n"));
  curlSrc->callPerform = TRUE;
}
/*______________________________________________________________________*/

void glibcurl_set_callback(GlibcurlCallback function, void* data) {
  g_source_set_callback(&curlSrc->source, (GSourceFunc)function, data,
                        NULL);
}
/*______________________________________________________________________*/

static gpointer selectThread(gpointer data) {
  int fdCount;
  struct timeval timeout;
  assert(data == 0); /* Just to get rid of unused param warning */

  D((stderr, "selectThread\n"));
  g_mutex_lock(curlSrc->mutex);
  D((stderr, "selectThread: got lock\n"));

  curlSrc->selectRunning = TRUE;
  while (curlSrc->selectRunning) {

    FD_ZERO(&curlSrc->fdRead);
    FD_ZERO(&curlSrc->fdWrite);
    FD_ZERO(&curlSrc->fdExc);
    curlSrc->fdMax = -1;
    /* What fds does libcurl want us to poll? */
    curl_multi_fdset(curlSrc->multiHandle, &curlSrc->fdRead,
                     &curlSrc->fdWrite, &curlSrc->fdExc, &curlSrc->fdMax);
    timeout.tv_sec = GLIBCURL_TIMEOUT / 1000;
    timeout.tv_usec = (GLIBCURL_TIMEOUT % 1000) * 1000;
    fdCount = select(curlSrc->fdMax + 1, &curlSrc->fdRead, &curlSrc->fdWrite,
                     &curlSrc->fdExc, &timeout);
    D((stderr, "selectThread: select() fdCount=%d\n", fdCount));

    g_atomic_int_inc(&curlSrc->gtkBlockAndWait); /* "GTK thread, block!" */
    D((stderr, "selectThread: waking up GTK thread %d\n",
       curlSrc->gtkBlockAndWait));
    /* GTK thread will almost immediately block in prepare() */
    g_main_context_wakeup(NULL);

    /* Now unblock GTK thread, continue after it signals us */
    D((stderr, "selectThread: pre-wait\n"));
    g_cond_wait(curlSrc->cond, curlSrc->mutex);
    D((stderr, "selectThread: post-wait\n"));

  }

  curlSrc->selectThread = NULL;
  D((stderr, "selectThread: exit\n"));
  g_mutex_unlock(curlSrc->mutex);
  return NULL;
}
/*______________________________________________________________________*/

/* Returning FALSE may cause the main loop to block indefinitely, but that is
   not a problem, we use g_main_context_wakeup to wake it up */
/* Returns TRUE iff it holds the mutex lock */
gboolean prepare(GSource* source, gint* timeout) {
  assert(source == &curlSrc->source);
  D((stderr, "prepare: callPerform=%d, thread=%p\n",
     curlSrc->callPerform, curlSrc->selectThread));

  *timeout = -1;

  if (g_atomic_int_dec_and_test(&curlSrc->gtkBlockAndWait)) {
    /* The select thread wants us to block */
    D((stderr, "prepare: trying lock\n"));
    g_mutex_lock(curlSrc->mutex);
    D((stderr, "prepare: got lock\n"));
    return TRUE;
  } else {
    g_atomic_int_inc(&curlSrc->gtkBlockAndWait);
  }

  /* Usual behaviour: Nothing happened, so don't dispatch. */
  if (!curlSrc->callPerform) return FALSE;

  /* Always dispatch if callPerform, i.e. 1st download just starting. */
  D((stderr, "prepare: trying lock 2\n"));
  /* Problem: We can block up to GLIBCURL_TIMEOUT msecs here, until the
     select() call returns. However, under Win32 this does not appear to be a
     problem (don't know why) - it _does_ tend to block the GTK thread under
     Linux. */
  g_mutex_lock(curlSrc->mutex);
  D((stderr, "prepare: got lock 2\n"));
  curlSrc->callPerform = FALSE;
  if (curlSrc->selectThread == NULL) {
    D((stderr, "prepare: starting select thread\n"));
    /* Note that the thread will stop soon because we hold mutex */
    curlSrc->selectThread = g_thread_create(&selectThread, 0, FALSE, NULL);
    assert(curlSrc->selectThread != NULL);
  }
  return TRUE;
}
/*______________________________________________________________________*/

/* Called after all the file descriptors are polled by glib. */
gboolean check(GSource* source) {
  assert(source == &curlSrc->source);
  return FALSE;
}
/*______________________________________________________________________*/

gboolean dispatch(GSource* source, GSourceFunc callback,
                  gpointer user_data) {
  CURLMcode x;
  int multiCount;

  assert(source == &curlSrc->source);
  do {
    x = curl_multi_perform(curlSrc->multiHandle, &multiCount);
    D((stderr, "dispatched: code=%d, reqs=%d\n", x, multiCount));
  } while (x == CURLM_CALL_MULTI_PERFORM);

  if (multiCount == 0)
    curlSrc->selectRunning = FALSE;

  if (callback != 0) (*callback)(user_data);

  /* Let selectThread call select() again */
  g_cond_signal(curlSrc->cond);
  g_mutex_unlock(curlSrc->mutex);

  return TRUE; /* "Do not destroy me" */
}
/*______________________________________________________________________*/

void finalize(GSource* source) {
  assert(source == &curlSrc->source);
}
/*======================================================================*/

#else /* !G_OS_WIN32 */

/* Number of highest allowed fd */
#define GLIBCURL_FDMAX 127

/* Timeout for the fds passed to glib's poll() call, in millisecs.
   curl_multi_fdset(3) says we should call curl_multi_perform() at regular
   intervals. */
#define GLIBCURL_TIMEOUT 1000

/* GIOCondition event masks */
#define GLIBCURL_READ  (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP)
#define GLIBCURL_WRITE (G_IO_OUT | G_IO_ERR | G_IO_HUP)
#define GLIBCURL_EXC   (G_IO_ERR | G_IO_HUP)

/** A structure which "derives" (in glib speak) from GSource */
typedef struct CurlGSource_ {
  GSource source; /* First: The type we're deriving from */

  CURLM* multiHandle;

  /* Previously seen FDs, for comparing with libcurl's current fd_sets */
  GPollFD lastPollFd[GLIBCURL_FDMAX + 1];
  int lastPollFdMax; /* Index of highest non-empty entry in lastPollFd */

  int callPerform; /* Non-zero => curl_multi_perform() gets called */

  /* For data returned by curl_multi_fdset */
  fd_set fdRead;
  fd_set fdWrite;
  fd_set fdExc;
  int fdMax;

} CurlGSource;

/* Global state: Our CurlGSource object */
static CurlGSource* curlSrc = 0;

/* The "methods" of CurlGSource */
static gboolean prepare(GSource* source, gint* timeout);
static gboolean check(GSource* source);
static gboolean dispatch(GSource* source, GSourceFunc callback,
                         gpointer user_data);
static void finalize(GSource* source);

static GSourceFuncs curlFuncs = {
  &prepare, &check, &dispatch, &finalize, 0, 0
};
/*______________________________________________________________________*/

void glibcurl_init() {
  int fd;
  /* Create source object for curl file descriptors, and hook it into the
     default main context. */
  curlSrc = (CurlGSource*)g_source_new(&curlFuncs, sizeof(CurlGSource));
  g_source_attach(&curlSrc->source, NULL);

  /* Init rest of our data */
  memset(&curlSrc->lastPollFd, 0, sizeof(curlSrc->lastPollFd));
  for (fd = 1; fd <= GLIBCURL_FDMAX; ++fd)
    curlSrc->lastPollFd[fd].fd = fd;
  curlSrc->lastPollFdMax = 0;
  curlSrc->callPerform = 0;

  /* Init libcurl */
  curl_global_init(CURL_GLOBAL_ALL);
  curlSrc->multiHandle = curl_multi_init();

  D((stderr, "events: R=%x W=%x X=%x\n", GLIBCURL_READ, GLIBCURL_WRITE,
     GLIBCURL_EXC));
}
/*______________________________________________________________________*/

CURLM* glibcurl_handle() {
  return curlSrc->multiHandle;
}
/*______________________________________________________________________*/

CURLMcode glibcurl_add(CURL *easy_handle) {
  assert(curlSrc->multiHandle != 0);
  curlSrc->callPerform = -1;
  return curl_multi_add_handle(curlSrc->multiHandle, easy_handle);
}
/*______________________________________________________________________*/

CURLMcode glibcurl_remove(CURL *easy_handle) {
  assert(curlSrc != 0);
  assert(curlSrc->multiHandle != 0);
  return curl_multi_remove_handle(curlSrc->multiHandle, easy_handle);
}
/*______________________________________________________________________*/

/* Call this whenever you have added a request using curl_multi_add_handle().
   This is necessary to start new requests. It does so by triggering a call
   to curl_multi_perform() even in the case where no open fds cause that
   function to be called anyway. */
void glibcurl_start() {
  curlSrc->callPerform = -1;
}
/*______________________________________________________________________*/

void glibcurl_set_callback(GlibcurlCallback function, void* data) {
  g_source_set_callback(&curlSrc->source, (GSourceFunc)function, data,
                        NULL);
}
/*______________________________________________________________________*/

void glibcurl_cleanup() {
  /* You must call curl_multi_remove_handle() and curl_easy_cleanup() for all
     requests before calling this. */
/*   assert(curlSrc->callPerform == 0); */

  curl_multi_cleanup(curlSrc->multiHandle);
  curlSrc->multiHandle = 0;
  curl_global_cleanup();

/*   g_source_destroy(&curlSrc->source); */
  g_source_unref(&curlSrc->source);
  curlSrc = 0;
}
/*______________________________________________________________________*/

static void registerUnregisterFds() {
  int fd, fdMax;

  FD_ZERO(&curlSrc->fdRead);
  FD_ZERO(&curlSrc->fdWrite);
  FD_ZERO(&curlSrc->fdExc);
  curlSrc->fdMax = -1;
  /* What fds does libcurl want us to poll? */
  curl_multi_fdset(curlSrc->multiHandle, &curlSrc->fdRead,
                   &curlSrc->fdWrite, &curlSrc->fdExc, &curlSrc->fdMax);
  /*fprintf(stderr, "registerUnregisterFds: fdMax=%d\n", curlSrc->fdMax);*/
  assert(curlSrc->fdMax >= -1 && curlSrc->fdMax <= GLIBCURL_FDMAX);

  fdMax = curlSrc->fdMax;
  if (fdMax < curlSrc->lastPollFdMax) fdMax = curlSrc->lastPollFdMax;

  /* Has the list of required events for any of the fds changed? */
  for (fd = 0; fd <= fdMax; ++fd) {
    gushort events = 0;
    if (FD_ISSET(fd, &curlSrc->fdRead))  events |= GLIBCURL_READ;
    if (FD_ISSET(fd, &curlSrc->fdWrite)) events |= GLIBCURL_WRITE;
    if (FD_ISSET(fd, &curlSrc->fdExc))   events |= GLIBCURL_EXC;

    /* List of events unchanged => no (de)registering */
    if (events == curlSrc->lastPollFd[fd].events) continue;

    D((stderr, "registerUnregisterFds: fd %d: old events %x, "
       "new events %x\n", fd, curlSrc->lastPollFd[fd].events, events));

    /* fd is already a lastPollFd, but event type has changed => do nothing.
       Due to the implementation of g_main_context_query(), the new event
       flags will be picked up automatically. */
    if (events != 0 && curlSrc->lastPollFd[fd].events != 0) {
      curlSrc->lastPollFd[fd].events = events;
      continue;
    }
    curlSrc->lastPollFd[fd].events = events;

    /* Otherwise, (de)register as appropriate */
    if (events == 0) {
      g_source_remove_poll(&curlSrc->source, &curlSrc->lastPollFd[fd]);
      curlSrc->lastPollFd[fd].revents = 0;
      D((stderr, "unregister fd %d\n", fd));
    } else {
      g_source_add_poll(&curlSrc->source, &curlSrc->lastPollFd[fd]);
      D((stderr, "register fd %d\n", fd));
    }
  }

  curlSrc->lastPollFdMax = curlSrc->fdMax;
}

/* Called before all the file descriptors are polled by the glib main loop.
   We must have a look at all fds that libcurl wants polled. If any of them
   are new/no longer needed, we have to (de)register them with glib. */
gboolean prepare(GSource* source, gint* timeout) {
  D((stderr, "prepare\n"));
  assert(source == &curlSrc->source);

  if (curlSrc->multiHandle == 0) return FALSE;

  registerUnregisterFds();

  *timeout = GLIBCURL_TIMEOUT;
/*   return FALSE; */
  return curlSrc->callPerform == -1 ? TRUE : FALSE;
}
/*______________________________________________________________________*/

/* Called after all the file descriptors are polled by glib.
   g_main_context_check() has copied back the revents fields (set by glib's
   poll() call) to our GPollFD objects. How inefficient all that copying
   is... let's add some more and copy the results of these revents into
   libcurl's fd_sets! */
gboolean check(GSource* source) {
  int fd, somethingHappened = 0;

  if (curlSrc->multiHandle == 0) return FALSE;

  assert(source == &curlSrc->source);
  FD_ZERO(&curlSrc->fdRead);
  FD_ZERO(&curlSrc->fdWrite);
  FD_ZERO(&curlSrc->fdExc);
  for (fd = 0; fd <= curlSrc->fdMax; ++fd) {
    gushort revents = curlSrc->lastPollFd[fd].revents;
    if (revents == 0) continue;
    somethingHappened = 1;
/*     D((stderr, "[fd%d] ", fd)); */
    if (revents & (G_IO_IN | G_IO_PRI))
      FD_SET((unsigned)fd, &curlSrc->fdRead);
    if (revents & G_IO_OUT)
      FD_SET((unsigned)fd, &curlSrc->fdWrite);
    if (revents & (G_IO_ERR | G_IO_HUP))
      FD_SET((unsigned)fd, &curlSrc->fdExc);
  }
/*   D((stderr, "check: fdMax %d\n", curlSrc->fdMax)); */

/*   return TRUE; */
/*   return FALSE; */
  return curlSrc->callPerform == -1 || somethingHappened != 0 ? TRUE : FALSE;
}
/*______________________________________________________________________*/

gboolean dispatch(GSource* source, GSourceFunc callback,
                  gpointer user_data) {
  CURLMcode x;

  assert(source == &curlSrc->source);
  assert(curlSrc->multiHandle != 0);
  do {
    x = curl_multi_perform(curlSrc->multiHandle, &curlSrc->callPerform);
/*     D((stderr, "dispatched %d\n", x)); */
  } while (x == CURLM_CALL_MULTI_PERFORM);

  /* If no more calls to curl_multi_perform(), unregister left-over fds */
  if (curlSrc->callPerform == 0) registerUnregisterFds();

  if (callback != 0) (*callback)(user_data);

  return TRUE; /* "Do not destroy me" */
}
/*______________________________________________________________________*/

void finalize(GSource* source) {
  assert(source == &curlSrc->source);
  registerUnregisterFds();
}

#endif
