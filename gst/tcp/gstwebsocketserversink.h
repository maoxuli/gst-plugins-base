/* 
 * Copyright (C) <2024> Maoxu Li <li@maoxuli.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef __GST_WEBSOCKET_SERVER_SINK_H__
#define __GST_WEBSOCKET_SERVER_SINK_H__


#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include <gio/gio.h>
#include <libsoup/soup.h>
#include <libsoup/soup-websocket-connection.h>

#define DEFAULT_HTML_ROOT "/var/www/html"

G_BEGIN_DECLS

#define GST_TYPE_WEBSOCKET_SERVER_SINK \
  (gst_websocket_server_sink_get_type())
#define GST_WEBSOCKET_SERVER_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBSOCKET_SERVER_SINK,GstWebsocketServerSink))
#define GST_WEBSOCKET_SERVER_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WEBSOCKET_SERVER_SINK,GstWebsocketServerSinkClass))
#define GST_IS_WEBSOCKET_SERVER_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBSOCKET_SERVER_SINK))
#define GST_IS_WEBSOCKET_SERVER_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WEBSOCKET_SERVER_SINK))

typedef struct _GstWebsocketServerSink GstWebsocketServerSink;
typedef struct _GstWebsocketServerSinkClass GstWebsocketServerSinkClass;

typedef enum {
  GST_WEBSOCKET_SERVER_SINK_CONNECTED = (GST_ELEMENT_FLAG_LAST << 0),
  GST_WEBSOCKET_SERVER_SINK_FLAG_LAST = (GST_ELEMENT_FLAG_LAST << 2),
} GstWebsocketServerSinkFlags;

struct _GstWebsocketServerSink {
  GstBaseSink element;

  /* server information */
  gchar *host;
  int port;
  gchar *html_root; 

  /* websocket server */
  SoupServer *server;
  SoupWebsocketConnection *connection; 
};

struct _GstWebsocketServerSinkClass {
  GstBaseSinkClass parent_class;
};

GType gst_websocket_server_sink_get_type(void);

G_END_DECLS

#endif /* __GST_WEBSOCKET_SERVER_SINK_H__ */
