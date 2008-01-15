/* $Id: glibcurl.h,v 1.7 2004/12/04 13:58:29 atterer Exp $ -*- C -*-
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

/** @file
    Use the libcurl multi interface from GTK+/glib programs without having to
    resort to multithreading */

#ifndef GLIBCURL_H
#define GLIBCURL_H

#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize libcurl. Call this once at the beginning of your program. This
    function makes calls to curl_global_init() and curl_multi_init() */
void glibcurl_init();

/** Return global multi handle */
CURLM* glibcurl_handle();

/** Convenience function, just executes
    curl_multi_add_handle(glibcurl_handle(), easy_handle); glibcurl_start()*/
CURLMcode glibcurl_add(CURL* easy_handle);

/** Convenience function, just executes
    curl_multi_remove_handle(glibcurl_handle(), easy_handle) */
CURLMcode glibcurl_remove(CURL* easy_handle);

/** Call this whenever you have added a request using
    curl_multi_add_handle(). This is necessary to start new requests. It does
    so by triggering a call to curl_multi_perform() even in the case where no
    open fds cause that function to be called anyway. The call happens
    "later", i.e. during the next iteration of the glib main loop.
    glibcurl_start() only sets a flag to make it happen. */
void glibcurl_start();

/** Callback function for glibcurl_set_callback */
typedef void (*GlibcurlCallback)(void*);
/** Set function to call after each invocation of curl_multi_perform(). Pass
    function==0 to unregister a previously set callback. The callback
    function will be called with the supplied data pointer as its first
    argument. */
void glibcurl_set_callback(GlibcurlCallback function, void* data);

/** You must call glibcurl_remove() and curl_easy_cleanup() for all requests
    before calling this. This function makes calls to curl_multi_cleanup()
    and curl_global_cleanup(). */
void glibcurl_cleanup();

#ifdef __cplusplus
}
#endif

#endif
