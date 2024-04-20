#include "BaseElementView.h"

#include <glib-object.h>  // for G_CALLBACK, g_signal_connect

#include "control/settings/Settings.h"  // for Settings
#include "gui/Shadow.h"                 // for Shadow
#include "util/Color.h"                 // for cairo_set_source_rgbi
#include "util/gtk4_helper.h"

#include "BackgroundSelectDialogBase.h"  // for BackgroundSelectDialogBase

BaseElementView::BaseElementView(size_t id, BackgroundSelectDialogBase* dlg): dlg(dlg), id(id) {
    this->widget = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(widget),
                                   GtkDrawingAreaDrawFunc(+[](GtkWidget*, cairo_t* cr, int, int, gpointer element) {
                                       static_cast<BaseElementView*>(element)->paint(cr);
                                   }),
                                   this, nullptr);

#if GTK_MAJOR_VERSION == 3
    gtk_widget_show(this->widget);
    gtk_widget_add_events(widget, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(this->widget, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton*, gpointer d) {
                         auto* element = static_cast<BaseElementView*>(d);
                         element->dlg->setSelected(element->id);
                         return true;
                     }),
                     this);
#else  // Set up right button clicks to pop up the context menu
    auto* ctrl = gtk_gesture_click_new();
    gtk_widget_add_controller(button.get(), GTK_EVENT_CONTROLLER(ctrl));
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 1);  // 1 = left button
    g_signal_connect(ctrl, "pressed",
                     G_CALLBACK(+[](GtkGestureClick* g, gint n_press, gdouble x, gdouble y, gpointer d) {
                         if (n_press == 1) {
                             auto* self = static_cast<SidebarPreviewBaseEntry*>(d);
                             self->mouseButtonPressCallback();
                             self->sidebar->openPreviewContextMenu(
                                     x, y, gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
                         }
                     }),
                     this);

#endif
}

BaseElementView::~BaseElementView() {
    gtk_widget_destroy(this->widget);

    if (this->crBuffer) {
        cairo_surface_destroy(this->crBuffer);
        this->crBuffer = nullptr;
    }
}

void BaseElementView::setSelected(bool selected) {
    if (this->selected == selected) {
        return;
    }
    this->selected = selected;

    repaint();
}

void BaseElementView::repaint() {
    if (this->crBuffer) {
        cairo_surface_destroy(this->crBuffer);
        this->crBuffer = nullptr;
    }
    gtk_widget_queue_draw(this->widget);
}

void BaseElementView::paint(cairo_t* cr) {
    GtkAllocation alloc;
    gtk_widget_get_allocation(this->widget, &alloc);

    if (this->crBuffer == nullptr) {
        this->crBuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, alloc.width, alloc.height);

        int width = getContentWidth();
        int height = getContentHeight();

        cairo_t* cr2 = cairo_create(this->crBuffer);

        cairo_set_source_rgba(cr2, 1, 1, 1, 0);
        cairo_rectangle(cr2, 0, 0, alloc.width, alloc.height);
        cairo_fill(cr2);

        cairo_matrix_t defaultMatrix = {0};
        cairo_get_matrix(cr2, &defaultMatrix);

        cairo_translate(cr2, Shadow::getShadowTopLeftSize() + 2, Shadow::getShadowTopLeftSize() + 2);

        paintContents(cr2);

        cairo_set_operator(cr2, CAIRO_OPERATOR_SOURCE);

        cairo_set_matrix(cr2, &defaultMatrix);

        cairo_set_operator(cr2, CAIRO_OPERATOR_ATOP);

        if (this->selected) {
            // Draw border
            Util::cairo_set_source_rgbi(cr2, dlg->getSettings()->getBorderColor());
            cairo_set_line_width(cr2, 2);
            cairo_set_line_cap(cr2, CAIRO_LINE_CAP_BUTT);
            cairo_set_line_join(cr2, CAIRO_LINE_JOIN_BEVEL);

            cairo_rectangle(cr2, Shadow::getShadowTopLeftSize() + 1.5, Shadow::getShadowTopLeftSize() + 1.5, width + 2,
                            height + 2);

            cairo_stroke(cr2);

            Shadow::drawShadow(cr2, Shadow::getShadowTopLeftSize(), Shadow::getShadowTopLeftSize(), width + 4,
                               height + 4);
        } else {
            Shadow::drawShadow(cr2, Shadow::getShadowTopLeftSize() + 2, Shadow::getShadowTopLeftSize() + 2, width,
                               height);
        }

        cairo_destroy(cr2);
    }

    cairo_set_source_surface(cr, this->crBuffer, 0, 0);
    cairo_paint(cr);
}

auto BaseElementView::getWidget() -> GtkWidget* {
    updateSize();
    return this->widget;
}

auto BaseElementView::getWidth() -> int {
    calcSize();
    return getContentWidth() + Shadow::getShadowBottomRightSize() + Shadow::getShadowTopLeftSize() + 4;
}

auto BaseElementView::getHeight() -> int {
    calcSize();
    return getContentHeight() + Shadow::getShadowBottomRightSize() + Shadow::getShadowTopLeftSize() + 4;
}

void BaseElementView::calcSize() {
    // Not implemented in the base class
}

void BaseElementView::updateSize() { gtk_widget_set_size_request(this->widget, this->getWidth(), this->getHeight()); }
