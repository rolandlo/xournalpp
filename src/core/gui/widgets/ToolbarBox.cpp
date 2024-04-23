#include "ToolbarBox.h"

#include "util/raii/GObjectSPtr.h"
#include "util/safe_casts.h"

G_BEGIN_DECLS
struct _ToolbarBox {
    GtkWidget widget;
    ToolbarBox* parent;
};

struct _ToolbarBoxClass {
    GtkWidgetClass parent_class;
};
G_END_DECLS

G_DEFINE_TYPE(_ToolbarBox, toolbarbox, GTK_TYPE_WIDGET);  // NOLINT // @suppress("Unused static function")

template <auto f, class... Args>
static void wrap(GtkWidget* w, Args... args) {
    (TOOLBAR_BOX(w)->parent->*f)(std::forward<Args>(args)...);
}

static void toolbarbox_class_init(_ToolbarBoxClass* klass) {
    GtkWidgetClass* widget_class = nullptr;

    widget_class = reinterpret_cast<GtkWidgetClass*>(klass);

    widget_class->snapshot = wrap<&ToolbarBox::snapshot>;
    widget_class->measure = wrap<&ToolbarBox::measure>;
    widget_class->size_allocate = wrap<&ToolbarBox::size_allocate>;

    gtk_widget_class_set_css_name(widget_class, "box");  // We want to catch themes tuning up box.toolbar
}

static void toolbarbox_init(_ToolbarBox* w) {
    w->parent = nullptr;
    gtk_widget_add_css_class(GTK_WIDGET(w), "toolbar");  // We want to catch themes tuning up box.toolbar
}

ToolbarBox::ToolbarBox(ToolbarSide side, int spacing):
        widget(TOOLBAR_BOX(g_object_new(toolbarbox_get_type(), nullptr)), xoj::util::adopt),
        overflowMenuButton(gtk_menu_button_new(), xoj::util::adopt),
        spacing(spacing),
        side(side) {
    widget->parent = this;
    gtk_widget_set_parent(overflowMenuButton.get(), getWidget());
    gtk_widget_set_can_focus(getWidget(), false);

    GtkWidget* popover = gtk_popover_new();
    gtk_widget_add_css_class(popover, "menu");
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), false);
    gtk_widget_set_halign(popover, GTK_ALIGN_END);
    gtk_widget_set_valign(popover, GTK_ALIGN_END);

    gtk_menu_button_set_popover(GTK_MENU_BUTTON(overflowMenuButton.get()), popover);
    setMenuButtonDirection(GTK_MENU_BUTTON(overflowMenuButton.get()), side);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    popoverBox = GTK_BOX(box);
    gtk_popover_set_child(GTK_POPOVER(popover), box);
}

static ToolbarSide fetchFromPlaceholder(GtkWidget* placeholder) {
    xoj_assert(GTK_IS_MENU_BUTTON(placeholder));
    switch (gtk_menu_button_get_direction(GTK_MENU_BUTTON(placeholder))) {
        case GTK_ARROW_DOWN:
            return ToolbarSide::TOP;
        case GTK_ARROW_UP:
            return ToolbarSide::BOTTOM;
        case GTK_ARROW_LEFT:
            return ToolbarSide::START;
        case GTK_ARROW_RIGHT:
            return ToolbarSide::END;
        default:
            g_warning("ToolbarBox::fetchFromPlaceholder() called on widget with GTK_ARROW_NONE");
            return ToolbarSide::TOP;
    }
}

ToolbarBox::ToolbarBox(GtkWidget* placeholder): ToolbarBox(fetchFromPlaceholder(placeholder)) {
    xoj_assert(GTK_IS_BOX(gtk_widget_get_parent(placeholder)));
    GtkBox* parent = GTK_BOX(gtk_widget_get_parent(placeholder));
    gtk_box_insert_child_after(parent, getWidget(), placeholder);
    gtk_box_remove(parent, placeholder);
}

ToolbarBox::~ToolbarBox() { gtk_widget_unparent(overflowMenuButton.get()); }

ToolbarBox::Child::Child(GtkWidget* widget, GtkWidget* proxy): widget(widget), proxy(proxy) {}
ToolbarBox::Child::~Child() {
    if (widget) {
        gtk_widget_unparent(widget);
    }
}

ToolbarBox::Child::Child(Child&& o): widget(std::exchange(o.widget, nullptr)), proxy(std::exchange(o.proxy, nullptr)) {}
auto ToolbarBox::Child::operator=(Child&& o) -> Child& {
    widget = std::exchange(o.widget, nullptr);
    proxy = std::exchange(o.proxy, nullptr);
    return *this;
}

GtkWidget* ToolbarBox::getWidget() const {
    xoj_assert(GTK_IS_WIDGET(widget.get()));
    return reinterpret_cast<GtkWidget*>(widget.get());
}

ToolbarSide ToolbarBox::getSide() const { return side; }

bool ToolbarBox::empty() const { return children.empty(); }

void ToolbarBox::clear() {
    children.clear();
    visibleChildren = 0;

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    popoverBox = GTK_BOX(box);
    gtk_popover_set_child(gtk_menu_button_get_popover(GTK_MENU_BUTTON(overflowMenuButton.get())), box);
}

void ToolbarBox::append(GtkWidget* w, GtkWidget* proxy) {
    this->children.emplace_back(w, proxy);
    gtk_widget_set_parent(w, getWidget());
    if (proxy) {
        gtk_widget_add_css_class(GTK_WIDGET(proxy), "model");
        gtk_box_append(popoverBox, GTK_WIDGET(proxy));
    }
}

inline static void setIfNonNull(int* p, int value) {
    if (p) {
        *p = value;
    }
}

void ToolbarBox::measure(GtkOrientation orientation, int for_size, int* minimum, int* natural, int* minimum_baseline,
                         int* natural_baseline) const {
    setIfNonNull(minimum_baseline, -1);
    setIfNonNull(natural_baseline, -1);

    if (children.empty()) {
        setIfNonNull(minimum, 0);
        setIfNonNull(natural, 0);
        return;
    }

    if (orientation == to_Orientation(this->side)) {
        int min = 0;
        int nat = 0;
        for (const auto& c: children) {
            int minChild, natChild;
            gtk_widget_measure(c.widget, orientation, -1, &minChild, &natChild, nullptr, nullptr);
            min += minChild;
            nat += natChild;
        }
        int spacings = strict_cast<int>(children.size() - 1) * spacing;

        // Ask for at least a third of the minimum. Overflow will go to the overflow menu
        setIfNonNull(minimum, (min + spacings) / 3);
        setIfNonNull(natural, nat + spacings);
    } else {
        int min = 0;
        int nat = 0;
        for (const auto& c: children) {
            int minChild, natChild;
            gtk_widget_measure(c.widget, orientation, -1, &minChild, &natChild, nullptr, nullptr);
            min = std::max(min, minChild);
            nat = std::max(nat, natChild);
        }
        setIfNonNull(minimum, min);
        setIfNonNull(natural, nat);
    }
}

void ToolbarBox::size_allocate(int width, int height, int baseline) {
    auto orientation = to_Orientation(this->side);
    int staticDim = orientation == GTK_ORIENTATION_HORIZONTAL ? height : width;
    int dynamicDim = orientation == GTK_ORIENTATION_HORIZONTAL ? width : height;

    int min = -spacing;
    std::vector<GtkRequestedSize> sizes;
    sizes.reserve(children.size());
    for (const auto& c: children) {
        auto& s = sizes.emplace_back(GtkRequestedSize{c.widget, 0, 0});
        gtk_widget_measure(c.widget, orientation, staticDim, &(s.minimum_size), &(s.natural_size), nullptr, nullptr);
        min += s.minimum_size + spacing;

        if (min > dynamicDim) {
            // An overflow menu is required. We don't need to know the size of the items left
            break;
        }
    }

    if (dynamicDim >= min) {
        // We have enough room to put all our widgets. Distribute the extra space between them.
        gtk_distribute_natural_allocation(dynamicDim - min, strict_cast<guint>(sizes.size()), sizes.data());
        gtk_widget_set_visible(overflowMenuButton.get(), false);
        visibleChildren = as_signed(children.size());
    } else {
        gtk_widget_set_visible(overflowMenuButton.get(), true);
        GtkRequestedSize sizeMenuButton = {overflowMenuButton.get(), 0, 0};
        gtk_widget_measure(overflowMenuButton.get(), orientation, staticDim, &(sizeMenuButton.minimum_size),
                           &(sizeMenuButton.natural_size), nullptr, nullptr);
        min += sizeMenuButton.minimum_size + spacing;

        // Remove last items so the overflow menu button fits
        while (min > dynamicDim && !sizes.empty()) {
            min -= sizes.back().minimum_size + spacing;
            sizes.pop_back();
        }

        // The Toolbar will only show the elements corresponding to an entry of `sizes`
        visibleChildren = as_signed(sizes.size());
        if (sizes.empty()) {
            g_warning(" ToolbarBox::size_allocate(): We don't have room for even one item. Abort!");
            return;
        }

        sizes.emplace_back(sizeMenuButton);
        gtk_distribute_natural_allocation(dynamicDim - min, strict_cast<guint>(sizes.size()), sizes.data());
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        if (gtk_widget_get_direction(GTK_WIDGET(widget.get())) == GTK_TEXT_DIR_LTR) {
            auto p = GRAPHENE_POINT_INIT_ZERO;
            int x = 0;
            for (const auto& s: sizes) {
                gtk_widget_allocate(GTK_WIDGET(s.data), s.minimum_size, height, -1,
                                    gsk_transform_translate(nullptr, &p));
                x += s.minimum_size + spacing;  // Sum as int to avoid rounding errors
                p.x = static_cast<float>(x);    // Shift the next widget
            }
        } else {  // Right-to-Left languages need the symmetric allocation
            auto p = GRAPHENE_POINT_INIT_ZERO;
            int x = width + spacing;  // The extra spacing will get substracted on first iteration
            for (const auto& s: sizes) {
                x -= s.minimum_size + spacing;  // Sum as int to avoid rounding errors
                p.x = static_cast<float>(x);
                gtk_widget_allocate(GTK_WIDGET(s.data), s.minimum_size, height, -1,
                                    gsk_transform_translate(nullptr, &p));
            }
        }
    } else {
        auto p = GRAPHENE_POINT_INIT_ZERO;
        int y = 0;
        for (const auto& s: sizes) {
            gtk_widget_allocate(GTK_WIDGET(s.data), width, s.minimum_size, -1, gsk_transform_translate(nullptr, &p));
            y += s.minimum_size + spacing;  // Sum as int to avoid rounding errors
            p.y = static_cast<float>(y);    // Shift the next widget
        }
    }

    for (auto it = children.begin(); it < std::next(children.begin(), visibleChildren); ++it) {
        if (it->proxy) {
            gtk_widget_set_visible(it->proxy, false);
        }
    }
    bool proxies = false;
    for (auto it = std::next(children.begin(), visibleChildren); it < children.end(); ++it) {
        // Move the overflowed widget out of sight.
        // Using gtk_widget_set_visible(w, false) does not work: it causes the gtk_widget_measure to return 0, causing
        // bugs upon resizing
        // Todo(anyone): find a better solution
        auto p = GRAPHENE_POINT_INIT(-100000.f, 0.f);
        gtk_widget_allocate(it->widget, 100, 100, -1, gsk_transform_translate(nullptr, &p));

        if (it->proxy) {
            gtk_widget_set_visible(it->proxy, true);
            proxies = true;
        }
    }
    gtk_widget_set_sensitive(overflowMenuButton.get(), proxies);
}

void ToolbarBox::snapshot(GtkSnapshot* sn) const {
    for (auto it = children.begin(); it < std::next(children.begin(), visibleChildren); ++it) {
        gtk_widget_snapshot_child(getWidget(), it->widget, sn);
    }
    if (gtk_widget_get_visible(overflowMenuButton.get())) {
        gtk_widget_snapshot_child(getWidget(), overflowMenuButton.get(), sn);
    }
}
