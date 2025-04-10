/*
 * Copyright (c) 2020 ericek111 <erik.brocko@letemsvetemapplem.eu>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Modified for edmcoverlay2. Such modifications are copyright © 2020 Ash Holland.

#include <iostream>
#include <stdlib.h>
#include <climits>
#include <chrono>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <math.h>

#include <fstream>
#include <stdio.h>

#include "socket.hh"
#include "json_message.hh"
#include "gason.h"
#include <csignal>

unsigned short port = 5020;

// Events for normal windows
#define BASIC_EVENT_MASK (StructureNotifyMask|ExposureMask|PropertyChangeMask|EnterWindowMask|LeaveWindowMask|KeyPressMask|KeyReleaseMask|KeymapStateMask)
#define NOT_PROPAGATE_MASK (KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ButtonMotionMask)

#define SCALE_W(x) ((int)((x) * window_width / 1280.0))
#define SCALE_H(y) ((int)((y) * window_height / 1024.0))
#define SCALE_X(x) (SCALE_W(x) + 20)
#define SCALE_Y(y) (SCALE_H(y) + 40)

using namespace std;

Display    *g_display;
XVisualInfo g_vinfo;
int         g_screen;
Window      g_win;
XftDraw    *g_draw;
int         g_disp_width;
int         g_disp_height;
/* Pixmap   g_bitmap; */
Colormap g_colormap;

XftColor red;
XftColor green;
XftColor yellow;
XftColor blue;
XftColor black;
XftColor white;
XftColor transparent; 

std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
int fpsmeterc = 0;
#define FPSMETERSAMPLE 100
auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
string fpsstring = "";

int     shape_event_base;
int     shape_error_base;

int window_xpos;
int window_ypos;
int window_width;
int window_height;

long event_mask = (StructureNotifyMask|ExposureMask|PropertyChangeMask|EnterWindowMask|LeaveWindowMask|KeyRelease | ButtonPress|ButtonRelease|KeymapStateMask);

void allow_input_passthrough (Window w) {
    XserverRegion region = XFixesCreateRegion (g_display, NULL, 0);

    //XFixesSetWindowShapeRegion (g_display, w, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion (g_display, w, ShapeInput, 0, 0, region);

    XFixesDestroyRegion (g_display, region);
}

void list_fonts() {
    char **fontlist;
    int num_fonts;
    fontlist = XListFonts (g_display, "*", 1000, &num_fonts);
    for (int i = 0; i < num_fonts; ++i) {
        fprintf(stderr, "> %s\n", fontlist[i]);
    }
}
// Create a XColor from 3 byte tuple (0 - 255, 0 - 255, 0 - 255).
XftColor createXftColorFromRGB(short red, short green, short blue) {
    XRenderColor color;
    XftColor xft_color;

    // m_color.red = red * 65535 / 255;
    color.red = (red * 0xFFFF) / 0xFF;
    color.green = (green * 0xFFFF) / 0xFF;
    color.blue = (blue * 0xFFFF) / 0xFF;
    color.alpha = 0xFFFF;

    if (!XftColorAllocValue(g_display, DefaultVisual(g_display, g_screen), g_colormap, &color, &xft_color)) {
        std::cerr << "createXftColorFromRGB: Cannot create color" << endl;
        exit(-1);
    }
    return xft_color;
}
// Create a XColor from 3 byte tuple (0 - 255, 0 - 255, 0 - 255).
XftColor createXftColorFromRGBA(short red, short green, short blue, short alpha) {
    XRenderColor color;
    XftColor xft_color;

    // m_color.red = red * 65535 / 255;
    color.red = (red * 0xFFFF) / 0xFF;
    color.green = (green * 0xFFFF) / 0xFF;
    color.blue = (blue * 0xFFFF) / 0xFF;
    color.alpha = (alpha * 0xFFFF) / 0xFF;

    if (!XftColorAllocValue(g_display, DefaultVisual(g_display, g_screen), g_colormap, &color, &xft_color)) {
        std::cerr << "createXftColorFromRGB: Cannot create color" << endl;
        exit(-1);
    }
    return xft_color;
}
XftColor parse_color(char* color_code) {
    XftColor color;
    if (color_code[0] == '#') {
        if (strlen(color_code) == 7)  {
            unsigned int r, g, b;
            sscanf(color_code, "#%2x%2x%2x", &r, &g, &b);
            color = createXftColorFromRGBA(r, g, b, 255);
        } else if (strlen(color_code) == 9) {
            unsigned int r, g, b, a;
            sscanf(color_code, "#%2x%2x%2x%2x", &a, &r, &g, &b);
            color = createXftColorFromRGBA(r, g, b, a);
        }
    } else if (strcmp(color_code, "red") == 0) {
        color = red;
    } else if (strcmp(color_code, "green") == 0) {
        color = green;
    } else if (strcmp(color_code, "yellow") == 0) {
        color = yellow;
    } else if (strcmp(color_code, "blue") == 0) {
        color = blue;
    } else if (strcmp(color_code, "black") == 0) {
        color = black;
    } else {
        color = white;
    }
    return color;
}
// Create a window
void createShapedWindow() {
    XSetWindowAttributes wattr;
    XftColor bgcolor = createXftColorFromRGBA(0, 0, 0, 0);

    Window root    = DefaultRootWindow(g_display);
    Visual *visual = DefaultVisual(g_display, g_screen);

    XMatchVisualInfo(g_display, DefaultScreen(g_display), 32, TrueColor, &g_vinfo);
    g_colormap = XCreateColormap(g_display, DefaultRootWindow(g_display), g_vinfo.visual, AllocNone);

    XSetWindowAttributes attr;
    attr.background_pixmap = None;
    attr.background_pixel = bgcolor.pixel;
    attr.border_pixel=0;
    attr.win_gravity=NorthWestGravity;
    attr.bit_gravity=ForgetGravity;
    attr.save_under=1;
    attr.event_mask=BASIC_EVENT_MASK;
    attr.do_not_propagate_mask=NOT_PROPAGATE_MASK;
    attr.override_redirect=1; // OpenGL > 0
    attr.colormap = g_colormap;

    unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWWinGravity|CWBitGravity | CWSaveUnder | CWDontPropagate | CWOverrideRedirect;

    g_win = XCreateWindow(g_display, root, window_xpos, window_ypos, window_width, window_height, 0, g_vinfo.depth, InputOutput, g_vinfo.visual, mask, &attr);

	/* g_bitmap = XCreateBitmapFromData (g_display, RootWindow(g_display, g_screen), (char *)myshape_bits, myshape_width, myshape_height); */

    //XShapeCombineMask(g_display, g_win, ShapeBounding, 900, 500, g_bitmap, ShapeSet);
    XShapeCombineMask(g_display, g_win, ShapeInput, 0, 0, None, ShapeSet );

    // We want shape-changed event too
    #define SHAPE_MASK ShapeNotifyMask
    XShapeSelectInput (g_display, g_win, SHAPE_MASK );

    // Tell the Window Manager not to draw window borders (frame) or title.
	wattr.override_redirect = 1;
    XChangeWindowAttributes(g_display, g_win, CWOverrideRedirect, &wattr);
    allow_input_passthrough(g_win);

    // Show the window
    XMapWindow(g_display, g_win);

    red = createXftColorFromRGBA(255, 0, 0, 255);
    green = createXftColorFromRGBA(0, 255, 0, 255);
    yellow = createXftColorFromRGBA(255, 255, 0, 255);
    blue = createXftColorFromRGBA(0, 0, 255, 255);
    black = createXftColorFromRGBA(0, 0, 0, 100);
    white = createXftColorFromRGBA(255, 255, 255, 255);
    transparent = createXftColorFromRGBA(0, 0, 0, 0);
}


void openDisplay() {
    g_display = XOpenDisplay(0);

    if (!g_display) {
        cerr << "Failed to open X display" << endl;
        exit(-1);
    }

    g_screen    = DefaultScreen(g_display);

    g_disp_width  = DisplayWidth(g_display, g_screen);
    g_disp_height = DisplayHeight(g_display, g_screen);

    // Has shape extions?
    if (!XShapeQueryExtension (g_display, &shape_event_base, &shape_error_base)) {
       cerr << "NO shape extension in your system !" << endl;
       exit (-1);
    }
}


enum class drawmode_t {
    idk,
    text,
    shape,
};

// NB: DO NOT FREE THESE
// they are pointers into request2
union drawitem_t {
    struct drawtext_t {
        // common
        drawmode_t drawmode;
        int x;
        int y;
        char* color;
        // text
        char* text;
        char* size;
    } text;
    struct drawshape_t {
        // common
        drawmode_t drawmode;
        int x;
        int y;
        char* color;
        // shape
        char* shape;
        char* fill;
        int w;
        int h;
	JsonNode* vect;
    } shape;
};


void sighandler(int signum) {
    cout << "edmcoverlay2: got signal " << signum << endl;
    if ((signum == SIGINT) || (signum == SIGTERM)) {
        cout << "edmcoverlay2: SIGINT/SIGTERM, exiting" << endl;
        XftDrawDestroy(g_draw);
        XDestroyWindow(g_display, g_win);
        XCloseDisplay(g_display);
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        cout << "Usage: overlay X Y W H" << endl;
        return 1;
    }
    window_xpos = atoi(argv[1]);
    window_ypos = atoi(argv[2]);
    window_width = atoi(argv[3]);
    window_height = atoi(argv[4]);
    cout << "edmcoverlay2: overlay starting up..." << endl;
    openDisplay();
    createShapedWindow();
    g_draw = XftDrawCreate(g_display, g_win, g_vinfo.visual, g_colormap);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    tcp_server_t server(port);

    {
        GC gc;
        XGCValues gcv;
        gc = XCreateGC(g_display, g_win, 0, 0);
        XSetBackground(g_display, gc, white.pixel);
        XSetForeground(g_display, gc, transparent.pixel);
        XFillRectangle(g_display, g_win, gc, 0, 0, window_width, window_height);
        const char *fontname = "Open Sans";
        XftFont *normalfont = XftFontOpen(g_display, g_screen,
            XFT_FAMILY, XftTypeString, fontname,
            XFT_SIZE, XftTypeDouble, 14.0,
            NULL);
        XSetForeground(g_display, gc, black.pixel);
        XFillRectangle(g_display, g_win, gc, 0, 0, 250, 100);
        const char* text = "edmcoverlay2 overlay process: running!";
        const unsigned char* utext = reinterpret_cast<const unsigned char *>(text);
        XftDrawStringUtf8(g_draw, &green, normalfont, SCALE_X(0), SCALE_Y(0) - 10, utext, strlen(text));
        XftFontClose(g_display, normalfont);
        XFreeGC(g_display, gc);
        XFlush(g_display);
    }

    cout << "edmcoverlay2: overlay ready." << endl;
    while (true) {
        socket_t socket = server.accept();
        std::string request = read_response(socket);
        char* request2 = strdup(request.c_str());
        /* cout << "edmcoverlay2: overlay got request: " << request << endl; */
        /* cout << "edmcoverlay2: overlay got request" << endl; */

        char* endptr;
        JsonValue value;
        JsonAllocator alloc;
        if (jsonParse(request2, &endptr, &value, alloc) != JSON_OK) {
            cout << "edmcoverlay2: bad json sent to overlay" << endl;
            free(request2);
            socket.close();
            continue;
        }

        GC gc;
        XGCValues gcv;
        gc = XCreateGC(g_display, g_win, 0, 0);
        XSetBackground(g_display, gc, white.pixel);

        const char *fontname = "Open Sans";
        XftFont *normalfont = XftFontOpen(g_display, g_screen,
            XFT_FAMILY, XftTypeString, fontname,
            XFT_SIZE, XftTypeDouble, 14.0,
            NULL);
        fontname = "Open Sans";
        const char *fontstyle = "Bold";
        XftFont *largefont = XftFontOpen(g_display, g_screen,
            XFT_FAMILY, XftTypeString, fontname,
            XFT_STYLE, XftTypeString, fontstyle,
            XFT_SIZE, XftTypeDouble, 20.0,
            NULL
        );

        XSetForeground(g_display, gc, transparent.pixel);
        XFillRectangle(g_display, g_win, gc, 0, 0, window_width, window_height);
        XSetForeground(g_display, gc, black.pixel);
        XFillRectangle(g_display, g_win, gc, 0, 0, 200, 50);
        XSetForeground(g_display, gc, white.pixel);
        const char* version = "edmcoverlay2 running";
        const unsigned char* u_text = reinterpret_cast<const unsigned char *>(version);
        XftDrawStringUtf8(g_draw, &white, normalfont, SCALE_X(0), SCALE_Y(0) - 10, u_text, strlen(version));

        int n = 0;
        for (auto v : value) {
            n++;
            /* cout << "edmcoverlay2: overlay processing graphics number " << std::to_string(++n) << endl; */
            /* text message: id, text, color, x, y, ttl, size
            * shape message: id, shape, color, fill, x, y, w, h, ttl
            * color: "red", "yellow", "green", "blue", "#rrggbb"
            * shape: "rect"
            * size: "normal", "large"
            */
            drawitem_t drawitem;
            XftColor main_color;
            for (JsonNode* node = v->value.toNode(); node != nullptr; node = node->next) {
                /* cout << "got key: " << node->key << endl; */
                // common
                if (strcmp(node->key ,"x") == 0) {
                    drawitem.text.x = node->value.toNumber();
                } else if (strcmp(node->key ,"y") == 0) {
                    drawitem.text.y = node->value.toNumber();
                } else if (strcmp(node->key ,"color") == 0) {
                    drawitem.text.color = node->value.toString();
                // text
                } else if (strcmp(node->key ,"text") == 0) {
                    drawitem.text.drawmode = drawmode_t::text;
                    drawitem.text.text = node->value.toString();
                } else if (strcmp(node->key ,"size") == 0) {
                    drawitem.text.drawmode = drawmode_t::text;
                    drawitem.text.size = node->value.toString();
                // shape
                } else if (strcmp(node->key ,"shape") == 0) {
                    drawitem.shape.drawmode = drawmode_t::shape;
                    drawitem.shape.shape = node->value.toString();
                } else if (strcmp(node->key ,"fill") == 0) {
                    drawitem.shape.drawmode = drawmode_t::shape;
                    drawitem.shape.fill = node->value.toString();
                } else if (strcmp(node->key ,"w") == 0) {
                    drawitem.shape.drawmode = drawmode_t::shape;
                    drawitem.shape.w = node->value.toNumber();
                } else if (strcmp(node->key ,"h") == 0) {
                    drawitem.shape.drawmode = drawmode_t::shape;
                    drawitem.shape.h = node->value.toNumber();
                } else if (strcmp(node->key, "vector") == 0) {
                    drawitem.shape.vect = node->value.toNode();
                } else {
                    cout << "bad key: " << node->key << endl;
                }
            }

            ///////////////// the part where we draw the thing

            if (drawitem.text.drawmode == drawmode_t::text) {
                /* cout << "edmcoverlay2: drawing a text" << endl; */
                main_color = parse_color(drawitem.text.color);
                u_text = reinterpret_cast<const unsigned char *>(drawitem.text.text);
                if (strcmp(drawitem.text.size, "large") == 0) {
                    XftDrawStringUtf8(g_draw, &main_color, largefont, SCALE_X(drawitem.text.x), SCALE_Y(drawitem.text.y), u_text, strlen(drawitem.text.text));
                } else {
                    XftDrawStringUtf8(g_draw, &main_color, normalfont, SCALE_X(drawitem.text.x), SCALE_Y(drawitem.text.y), u_text, strlen(drawitem.text.text));
                }
            } else {
                /* cout << "edmcoverlay2: drawing a shape" << endl; */
                main_color = parse_color(drawitem.shape.color);
                XSetForeground(g_display, gc, main_color.pixel);
                if (strcmp(drawitem.shape.shape, "rect") == 0) {
                    /* cout << "edmcoverlay2: specifically, a rect" << endl; */
                    // TODO distinct fill/edge colour
                    XDrawRectangle(g_display, g_win, gc, SCALE_X(drawitem.shape.x), SCALE_Y(drawitem.shape.y), SCALE_W(drawitem.shape.w), SCALE_H(drawitem.shape.h));
                } else if (strcmp(drawitem.shape.shape, "vect") == 0) {
                    /* cout << "edmcoverlay2: specifically, a vect" << endl; */
                    // TODO: make this less gross
#define UNINIT_COORD 1000000
                    int x1 = UNINIT_COORD, y1 = UNINIT_COORD, x2 = UNINIT_COORD, y2 = UNINIT_COORD;
                    JsonNode* vect_ = drawitem.shape.vect;
                    for (JsonNode* node_ = vect_; node_ != nullptr; node_ = node_->next) {
                        // node_ is a point
                        int x, y;
                        for (auto z : node_->value) {
                            if (strcmp(z->key, "x") == 0) {
                                x = z->value.toNumber();
                            } else if (strcmp(z->key, "y") == 0) {
                                y = z->value.toNumber();
                            }
                        }
                        if (x1 == UNINIT_COORD) {
                            x1 = x;
                            y1 = y;
                            continue;
                        }
                        if (x2 == UNINIT_COORD) {
                            x2 = x;
                            y2 = y;
                            XDrawLine(g_display, g_win, gc, SCALE_X(x1), SCALE_Y(y1), SCALE_X(x2), SCALE_Y(y2));
                            continue;
                        }
                        x1 = x2;
                        y1 = y2;
                        x2 = x;
                        y2 = y;
                        XDrawLine(g_display, g_win, gc, SCALE_X(x1), SCALE_Y(y1), SCALE_X(x2), SCALE_Y(y2));
                    }
                }
            }
            XftColorFree(g_display, g_vinfo.visual, g_colormap, &main_color);
        }

        /* cout << "edmcoverlay2: done drawing " << std::to_string(n) << " graphics" << endl; */

        XFreeGC(g_display, gc);
        XFlush(g_display);
        
        XftFontClose(g_display, normalfont);
        XftFontClose(g_display, largefont);

        free(request2);
        socket.close();
    }
    XftDrawDestroy(g_draw);
    XDestroyWindow(g_display, g_win);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &red);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &green);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &yellow);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &blue);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &black);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &white);
    XftColorFree(g_display, g_vinfo.visual, g_colormap, &transparent);
    XCloseDisplay(g_display);
    server.close();
    return 0;
}

