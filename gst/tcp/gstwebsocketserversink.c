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

/**
 * SECTION:element-websocketserversink
 * @title: websocketserversink
 * @see_also: #httpserversink
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst-i18n-plugin.h>

#include "gsttcpelements.h"
#include "gstwebsocketserversink.h"

#include <glib/gstdio.h>

/* websocketserverSink signals and args */
enum
{
  CONNECTED,
  DISCONNECTED,
  LAST_SIGNAL
};

GST_DEBUG_CATEGORY_STATIC (websocketserversink_debug);
#define GST_CAT_DEFAULT (websocketserversink_debug)

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_HTML_ROOT
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_websocket_server_sink_finalize (GObject * object);
static void gst_websocket_server_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_websocket_server_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_websocket_server_sink_start (GstBaseSink * bsink);
static gboolean gst_websocket_server_sink_stop (GstBaseSink * bsink);
static GstFlowReturn gst_websocket_server_sink_render (GstBaseSink * bsink,
    GstBuffer * buf);

/*static guint gst_websocket_server_sink_signals[LAST_SIGNAL] = { 0 }; */

#define gst_websocket_server_sink_parent_class parent_class
G_DEFINE_TYPE (GstWebsocketServerSink, gst_websocket_server_sink,
    GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (websocketserversink,
    "websocketserversink", GST_RANK_NONE, GST_TYPE_WEBSOCKET_SERVER_SINK,
    tcp_element_init (plugin));

static void
gst_websocket_server_sink_class_init (GstWebsocketServerSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_websocket_server_sink_finalize;
  gobject_class->set_property = gst_websocket_server_sink_set_property;
  gobject_class->get_property = gst_websocket_server_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host", "The host/IP to send the packets to",
          TCP_DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "The port to send the packets to",
          0, TCP_HIGHEST_PORT, TCP_DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HTML_ROOT,
      g_param_spec_string ("html_root", "HTML Root",
          "The root directory for html files", DEFAULT_HTML_ROOT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gst_element_class_set_static_metadata (gstelement_class,
      "Websocket server sink", "Sink/Network",
      "Send data as a server over the network via websocket",
      "Maoxu Li <li@maoxuli.com>");

  gstbasesink_class->start = gst_websocket_server_sink_start;
  gstbasesink_class->stop = gst_websocket_server_sink_stop;
  gstbasesink_class->render = gst_websocket_server_sink_render;

  GST_DEBUG_CATEGORY_INIT (websocketserversink_debug, "websocketserversink", 0,
      "websocket sink");
}

static void
gst_websocket_server_sink_init (GstWebsocketServerSink * this)
{
  this->host = g_strdup (TCP_DEFAULT_HOST);
  this->port = TCP_DEFAULT_PORT;
  this->html_root = g_strdup (DEFAULT_HTML_ROOT);

  this->server = NULL;
  this->connection = NULL;

  GST_OBJECT_FLAG_UNSET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED);
}

static void
gst_websocket_server_sink_finalize (GObject * object)
{
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (object);
  g_return_if_fail (GST_IS_WEBSOCKET_SERVER_SINK (object));

  g_free (this->host);
  g_free (this->html_root);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_websocket_server_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (object);
  g_return_if_fail (GST_IS_WEBSOCKET_SERVER_SINK (object));

  switch (prop_id) {
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (this->host);
      this->host = g_value_dup_string (value);
      break;
    case PROP_PORT:
      this->port = g_value_get_int (value);
      break;
    case PROP_HTML_ROOT:
      if (!g_value_get_string (value)) {
        g_warning ("HTML root cannot be NULL");
        break;
      }
      g_free (this->html_root);
      this->html_root = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_websocket_server_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (object);
  g_return_if_fail (GST_IS_WEBSOCKET_SERVER_SINK (object));

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, this->host);
      break;
    case PROP_PORT:
      g_value_set_int (value, this->port);
      break;
    case PROP_HTML_ROOT:
      g_value_set_string (value, this->html_root);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
http_message (SoupServer * server, SoupServerMessage * msg, const char *path,
    GHashTable * query, gpointer user_data)
{
  GstWebsocketServerSink *this = (GstWebsocketServerSink *) user_data;
  GST_DEBUG_OBJECT (this, "received http message for path: %s", path);

  GST_DEBUG_OBJECT (this, "method: %s", soup_server_message_get_method (msg));
  if (soup_server_message_get_method (msg) == SOUP_METHOD_GET
      || soup_server_message_get_method (msg) == SOUP_METHOD_HEAD) {
    GStatBuf st;
    char *file_path = g_strdup_printf ("%s%s", this->html_root, path);
    GST_DEBUG_OBJECT (this, "file path: %s", file_path);
    if (g_stat (file_path, &st) == -1) {
      if (errno == EPERM) {
        GST_DEBUG_OBJECT (this, "file is forbidden");
        soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
      } else if (errno == ENOENT) {
        GST_DEBUG_OBJECT (this, "file is not found");
        soup_server_message_set_status (msg, SOUP_STATUS_NOT_FOUND, NULL);
      } else {
        GST_DEBUG_OBJECT (this, "error to have file stat");
        soup_server_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR,
            NULL);
      }
    } else if (!g_file_test (file_path, G_FILE_TEST_IS_REGULAR)) {
      GST_DEBUG_OBJECT (this, "path is not for a regular file");
      soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
    } else if (soup_server_message_get_method (msg) == SOUP_METHOD_GET) {
      GBytes *buffer = NULL;
      GMappedFile *mapping = g_mapped_file_new (file_path, FALSE, NULL);
      if (!mapping) {
        GST_DEBUG_OBJECT (this, "failed mapping file for contents");
        soup_server_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR,
            NULL);
      } else {
        buffer =
            g_bytes_new_with_free_func (g_mapped_file_get_contents (mapping),
            g_mapped_file_get_length (mapping),
            (GDestroyNotify) g_mapped_file_unref, mapping);
        GST_DEBUG_OBJECT (this, "response with file contents: %lu",
            g_bytes_get_size (buffer));
        soup_message_body_append_bytes (soup_server_message_get_response_body
            (msg), buffer);
        g_bytes_unref (buffer);
        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
      }
    } else if (soup_server_message_get_method (msg) == SOUP_METHOD_HEAD) {
      char *length = g_strdup_printf ("%lu", (gulong) st.st_size);
      GST_DEBUG_OBJECT (this, "response with content length: %s", length);
      soup_message_headers_append (soup_server_message_get_response_headers
          (msg), "Content-Length", length);
      g_free (length);
      soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    }
  }
}

// static void
// websocket_disconnected (SoupWebsocketConnection * connection, gpointer user_data)
// {
//   GstWebsocketServerSink *this;
//   this = (GstWebsocketServerSink*) user_data;
//   GST_DEBUG_OBJECT (this, "websocket disconnected");

//   if (this->connection == connection) {
//     GST_DEBUG_OBJECT (this, "remove websocket connection");
//     g_object_unref (this->connection);
//     this->connection = NULL;
//     GST_OBJECT_FLAG_UNSET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED);
//   }
// }

static void
websocket_connected (SoupServer * server, SoupServerMessage * msg,
    const char *path, SoupWebsocketConnection * connection, gpointer user_data)
{
  GstWebsocketServerSink *this = (GstWebsocketServerSink *) user_data;
  GST_DEBUG_OBJECT (this, "websocket connected");

  if (this->connection) {
    GST_DEBUG_OBJECT (this, "disconnect websocket connection");
    soup_websocket_connection_close (this->connection, 0, 0);
    g_object_unref (this->connection);
    this->connection = NULL;
    GST_OBJECT_FLAG_UNSET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED);
  }

  GST_DEBUG_OBJECT (this, "add websocket connection");
  this->connection = g_object_ref (connection);
  // g_signal_connect (this->connection, "disconnected", G_CALLBACK (websocket_disconnected), this);
  GST_OBJECT_FLAG_SET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED);
}

/* create a http and websocket server */
static gboolean
gst_websocket_server_sink_start (GstBaseSink * bsink)
{
  GError *error = NULL;
  GSocketAddress *address = NULL;
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (bsink);
  GST_INFO_OBJECT (this, "starting server on %s:%d", this->host, this->port);

  address = g_inet_socket_address_new_from_string (this->host, this->port);
  if (!address)
    goto name_resolve;

  g_assert (!this->server);
  this->server = soup_server_new ("server-header", "websocket-server ", NULL);
  g_assert (this->server);

  soup_server_remove_websocket_extension (this->server,
      SOUP_TYPE_WEBSOCKET_EXTENSION_DEFLATE);
  if (!soup_server_listen (this->server, address, 0, &error))
    goto listen_failed;

  g_object_unref (address);
  if (error)
    goto server_error;

  soup_server_add_handler (this->server, "/", http_message, this, NULL);
  GST_INFO_OBJECT (this, "websocket service on %s:%d/mjpeg", this->host,
      this->port);
  soup_server_add_websocket_handler (this->server, "/mjpeg", NULL, NULL,
      websocket_connected, this, NULL);
  GST_DEBUG_OBJECT (this, "server started");
  return TRUE;

name_resolve:
  {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (this, "Cancelled name resolution");
    } else {
      GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ, (NULL),
          ("Failed to resolve host '%s': %s", this->host, error->message));
    }
    g_clear_error (&error);
    return FALSE;
  }
listen_failed:
  {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (this, "Cancelled binding");
    } else {
      GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ, (NULL),
          ("Failed to bind on host '%s:%d': %s", this->host, this->port,
              error->message));
    }
    g_clear_error (&error);
    g_object_unref (address);
    soup_server_disconnect (this->server);
    this->server = NULL;
    return FALSE;
  }
server_error:
  {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (this, "Cancelled listening");
    } else {
      GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ, (NULL),
          ("Failed to listen on host '%s:%d': %s", this->host, this->port,
              error->message));
    }
    g_clear_error (&error);
    soup_server_disconnect (this->server);
    this->server = NULL;
    return FALSE;
  }
}

static gboolean
gst_websocket_server_sink_stop (GstBaseSink * bsink)
{
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (bsink);
  GST_DEBUG_OBJECT (this, "stopping server");

  if (this->connection) {
    GST_DEBUG_OBJECT (this, "closing connection");
    soup_websocket_connection_close (this->connection, 0, 0);
    g_object_unref (this->connection);
    this->connection = NULL;
    GST_OBJECT_FLAG_UNSET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED);
  }

  if (this->server) {
    GST_DEBUG_OBJECT (this, "closing server");
    soup_server_disconnect (this->server);
    g_clear_object (&this->server);
  }

  GST_DEBUG_OBJECT (this, "server stopped");
  return TRUE;
}

static GstFlowReturn
gst_websocket_server_sink_render (GstBaseSink * bsink, GstBuffer * buf)
{
  GstMapInfo map;
  GstWebsocketServerSink *this = GST_WEBSOCKET_SERVER_SINK (bsink);

  if (GST_OBJECT_FLAG_IS_SET (this, GST_WEBSOCKET_SERVER_SINK_CONNECTED)) {
    gst_buffer_map (buf, &map, GST_MAP_READ);
    // GST_LOG_OBJECT (this, "writing %" G_GSIZE_FORMAT " bytes", map.size);
    soup_websocket_connection_send_binary (this->connection, map.data,
        map.size);
    gst_buffer_unmap (buf, &map);
  }
  return GST_FLOW_OK;
}
