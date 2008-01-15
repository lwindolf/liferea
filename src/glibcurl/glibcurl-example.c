/* $Id: glibcurl-example.c,v 1.5 2004/12/04 13:57:06 atterer Exp $ -*- C -*-
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

#include <stdio.h>

#include <glibcurl.h>
#include <glib.h>

#define HANDLES 2

int numRunning = HANDLES;

unsigned nBytes = 0; /* to accumulate total nr of bytes downloaded */

size_t curlWriter(void *ptr, size_t size, size_t nmemb, void *stream);
void curlCallback(void*);

int main(int argc, char** argv) {
  CURL* h[HANDLES];
  GMainLoop* gloop;
  int i;

  if (argc < 2) {
    printf("Invoke this as \"%s http://URL\". %d concurrent transfers of "
           "the URL will be started, one printing AAA, the next BBB...\n",
           argv[0], HANDLES);
    return 1;
  }

  glibcurl_init();
  gloop = g_main_loop_new(NULL, TRUE);
  glibcurl_set_callback(&curlCallback, gloop);

  for (i = 0; i < HANDLES; ++i) {
    h[i] = curl_easy_init();
    curl_easy_setopt(h[i], CURLOPT_URL, argv[1]);
    curl_easy_setopt(h[i], CURLOPT_VERBOSE, 1);
    curl_easy_setopt(h[i], CURLOPT_BUFFERSIZE, 1024*10);
    curl_easy_setopt(h[i], CURLOPT_WRITEFUNCTION, curlWriter);
    curl_easy_setopt(h[i], CURLOPT_WRITEDATA, "ABCDEFGHIJKLMN" + i);
    glibcurl_add(h[i]);
  }

  /* Run main loop. Alternatively, you can use gtk_main() */
  printf("Start\n");
  g_main_loop_run(gloop);
  printf("\nFinished, fetched %u bytes\n", nBytes);

  /* Clean up */
  for (i = 0; i < HANDLES; ++i) {
    glibcurl_remove(h[i]);
    curl_easy_cleanup(h[i]);
  }
  glibcurl_cleanup();

  return 0;
}

size_t curlWriter(void* ptr, size_t size, size_t nmemb, void *stream) {
  if (ptr == 0) return 0; /* NOP, just to avoid "unused param" warning */
  putchar(*(char*)stream); fflush(stdout);
  nBytes += size * nmemb;
  return size * nmemb;
}

void curlCallback(void* data) {
  CURLMsg* msg;
  int inQueue;

  /*   putchar(' '); */
  while (1) {
    msg = curl_multi_info_read(glibcurl_handle(), &inQueue);
    if (msg == 0) break;
    if (msg->msg != CURLMSG_DONE) continue;
    /* Cause the above call to g_main_loop_run() to terminate once all
       requests are finished. */
    if (--numRunning == 0) g_main_loop_quit((GMainLoop*)data);
  }
}
