
#define  SK_VULKAN
#include <skia/include/core/SkImage.h>
#include <skia/include/gpu/vk/GrVkBackendContext.h>
#include <skia/include/gpu/GrBackendSurface.h>
#include <skia/include/gpu/GrDirectContext.h>
#include <skia/include/gpu/gl/GrGLInterface.h>
#include <skia/include/core/SkFont.h>
#include <skia/include/core/SkCanvas.h>
#include <skia/include/core/SkColorSpace.h>
#include <skia/include/core/SkSurface.h>
#include <skia/include/core/SkFontMgr.h>
#include <skia/include/core/SkFontMetrics.h>
#include <skia/include/core/SkPathMeasure.h>
#include <skia/include/core/SkTextBlob.h>
#include <skia/include/effects/SkGradientShader.h>
#include <skia/include/effects/SkImageFilters.h>
#include <skia/include/effects/SkDashPathEffect.h>

#include <ux/ux.hpp>
#include <ux/canvas.hpp>
#include <media/media.hpp>
#include <vk/vk.hpp>

using namespace ion;


inline SkColor sk_color(rgbad c) {
    rgba8 i = { math::round(c.r * 255), math::round(c.g * 255), math::round(c.b * 255), math::round(c.a * 255) };
    auto sk = SkColor(uint32_t(i.b)        | (uint32_t(i.g) << 8) |
                     (uint32_t(i.r) << 16) | (uint32_t(i.a) << 24));
    return sk;
}

inline SkColor sk_color(rgba8 c) {
    auto sk = SkColor(uint32_t(c.b)        | (uint32_t(c.g) << 8) |
                     (uint32_t(c.r) << 16) | (uint32_t(c.a) << 24));
    return sk;
}

struct Skia {
    sk_sp<GrDirectContext> sk_context;

    Skia(sk_sp<GrDirectContext> sk_context) : sk_context(sk_context) { }
    
    static Skia *Context(VkEngine e) {
        static struct Skia *sk = null;
        if (sk) return sk;

        //GrBackendFormat gr_conv = GrBackendFormat::MakeVk(VK_FORMAT_R8G8B8_SRGB);
        sk_sp<GrDirectContext> sk_context;
        
        GrVkBackendContext grc {
            e->vk->inst(),
            e->vk_gpu->phys,
            e->vk_device->device,
            e->vk_device->graphicsQueue,
            e->vk_gpu->indices.graphicsFamily.value(),
            e->vk->version
        };

        grc.fMaxAPIVersion = e->vk->version;


        //grc.fVkExtensions = new GrVkExtensions(); // internal needs population perhaps
        grc.fGetProc = [](cchar_t *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
            return (dev == VK_NULL_HANDLE) ? vkGetInstanceProcAddr(inst, name) :
                                                vkGetDeviceProcAddr  (dev,  name);
        };

        sk_context = GrDirectContext::MakeVulkan(grc);

        assert(sk_context);
        sk = new Skia(sk_context);
        return sk;
    }
};

struct ICanvas {
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    VkhImage           image = null;

    struct state {
        image       img;
        double      outline_sz;
        double      font_scale;
        double      opacity;
        m44d        m;
        rgbad       color;
        shape       clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
    };

    state *top = null;
    doubly<SkPaint> stack;

    ICanvas(VkEngine &e, VkhImage &image, vec2i &sz) : e(e), sz(sz) {
        resize(image, sz.x, sz.y);
    }

    void color(rgbad &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    void resize(VkhImage &image, int width, int height) {
        if (!image) {
            image = vkh_image_create(
                e->vkh, VK_FORMAT_B8G8R8A8_UNORM, u32(width), u32(height),
                VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
        this->image = vkh_image_grab(image);
        sz = vec2i { width, height };
        ///
        GrDirectContext *ctx    = Skia::Context()->sk_context.get();
        auto imi                = GrVkImageInfo { };
        imi.fImage              = image->image;
        imi.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
        imi.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imi.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    ///imi.fImageUsageFlags    = VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // i dont think so.
        imi.fSampleCount        = 1;
        imi.fLevelCount         = 1;
        imi.fCurrentQueueFamily = e->gpu->graphicsFamily.value();
        imi.fProtected          = GrProtected::kNo;
        imi.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        auto rt = GrBackendRenderTarget { sz.x, sz.y, imi };
        sk_surf = SkSurface::MakeFromBackendRenderTarget(ctx, rt,
                    kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, null, null);
        sk_canvas = sk_surf->getCanvas();
        
        /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
        //float sx, sy;
        //vkh_presenter_get_size(data->vkh_renderer, &width, &height, &sx, &sy); /// vkh should have both vk engine and glfw facility
        //data->width  /= sx; /// we use the virtual size here
        //data->height /= sy;

        vkh_presenter_build_blit_cmd(data->vkh_renderer, image->image, width, height);
        
        assert(data->stack->len() <= 1);

        while (data->stack)
            data->stack->pop();
        data->stack->push();
        data->top = &data->stack->last();
    }

    SkPath *sk_path(graphics::shape &sh) {
        graphics::shape::sdata &shape = sh.ref<graphics::shape::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rectd)) {
                rectd &m = mem->ref<rectd>();
                SkRect r = SkRect {
                    m.x, m.y, m.x + m.w, m.y + m.h
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded<double>)) {
                Rounded<double>::rdata &m = mem->ref<Rounded<double>::rdata>();
                p.moveTo  (m.tl_x.x, m.tl_x.y);
                p.lineTo  (m.tr_x.x, m.tr_x.y);
                p.cubicTo (m.c0.x,   m.c0.y, m.c1.x, m.c1.y, m.tr_y.x, m.tr_y.y);
                p.lineTo  (m.br_y.x, m.br_y.y);
                p.cubicTo (m.c0b.x,  m.c0b.y, m.c1b.x, m.c1b.y, m.br_x.x, m.br_x.y);
                p.lineTo  (m.bl_x.x, m.bl_x.y);
                p.cubicTo (m.c0c.x,  m.c0c.y, m.c1c.x, m.c1c.y, m.bl_y.x, m.bl_y.y);
                p.lineTo  (m.tl_y.x, m.tl_y.y);
                p.cubicTo (m.c0d.x,  m.c0d.y, m.c1d.x, m.c1d.y, m.tl_x.x, m.tl_x.y);
            } else {
                graphics::shape::sdata &m = mem->ref<graphics::shape::sdata>();
                for (mx &o:m.ops) {
                    type_t t = o.type();
                    if (t == typeof(Movement)) {
                        Movement m(o);
                        p.moveTo(m->x, m->y);
                    } else if (t == typeof(Vec2<double>)) {
                        Vec2<double> l(o);
                        p.lineTo(l->x, l->y);
                    }
                }
                p.close();
            }
        }

        /// issue here is reading the data, which may not be 'sdata', but Rect, Rounded
        /// so the case below is 
        if (bool(shape.sk_offset) && o_cache == o)
            return shape.sk_offset;
        ///
        if (!std::isnan(o) && o != 0) {
            assert(shape.sk_path); /// must have an actual series of shape operations in skia
            ///
            delete sk_offset;
            sk_offset = new SkPath(*shape.sk_path);
            o_cache = o;
            ///
            SkPath  fpath;
            SkPaint cp = SkPaint(ps);
            cp.setStyle(SkPaint::kStroke_Style);
            cp.setStrokeWidth(std::abs(o) * 2);
            cp.setStrokeJoin(SkPaint::kRound_Join);
            cp.getFillPath((const SkPath &)*shape.sk_path, &fpath);
            
            auto vrbs = shape.sk_path->countVerbs();
            auto pnts = shape.sk_path->countPoints();
            std::cout << "sk_path = " << (void *)shape.sk_path << ", pointer = " << (void *)this << " verbs = " << vrbs << ", points = " << pnts << "\n";
            ///
            if (o < 0) {
                sk_offset->reverseAddPath(fpath);
                last_offset->setFillType(SkPathFillType::kWinding);
            } else
                last_offset->addPath(fpath);
            ///
            //output->p->toggleInverseFillType();
            return last_offset;
        }
        return shape.sk_path;
    }

    void font(ion::font &f) { 
        top->font = f;
    }

    void save() {
        state &s = stack.push();
        if (top) {
            s = *top;
        } else {
            s.ps = SkPaint { };
        }
        sk_canvas->save();
        top = &s;
    }

    void    clear()        { sk_canvas->clear(sk_color(top->color)); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        sk_canvas->flush();
    }

    void  restore() {
        stack.pop();
        top = stack.len() ? &stack.last() : null;
        sk_canvas->restore();
    }

    vec2i size() { return sz; }

    /// console would just think of everything in char units. like it is.
    /// measuring text would just be its length, line height 1.
    text_metrics measure(str &text) {
        SkFontMetrics mx;
        SkFont     &font = top->font.handle();
        auto         adv = font.measureText(text.cstr(), text.size(), SkTextEncoding::kUTF8);
        auto          lh = font.getMetrics(&mx);
        return text_metrics {
            .w           = adv,
            .h           = abs(mx.fAscent) + abs(mx.fDescent),
            .ascent      = mx.fAscent,
            .descent     = mx.fDescent,
            .line_height = lh,
            .cap_height  = mx.fCapHeight
        };
    }

    /// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
    /// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
    /// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone
    str ellipsis(str &text, rectd &rect, text_metrics &tm) {
        const str el = "...";
        str       cur, *p = &text;
        int       trim = p->size();
        tm             = measure((str &)el);
        
        if (tm.w >= rect.w)
            trim = 0;
        else
            for (;;) {
                tm = measure(*p);
                if (tm.w <= rect.w || trim == 0)
                    break;
                if (tm.w > rect.w && trim >= 1) {
                    cur = text.substr(0, --trim) + el;
                    p   = &cur;
                }
            }
        return (trim == 0) ? "" : (p == &text) ? text : cur;
    }

    void image(ion::image &image, rectd &rect, alignment &align, vec2 &offset) {
        State   *s = (State *)top->b_state; // backend state (this)
        SkPaint ps = SkPaint(s->ps);
        vec2   pos = { 0, 0 };
        vec2i  isz = image.size();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// cache management;
        if (!image.pixels.attachments()) {
            SkBitmap bm;
            rgba    *px = image.pixels.data<rgba>();
            SkImageInfo info = SkImageInfo::Make(isz.x, isz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
            bm.installPixels(info, px, image.stride());
            sk_sp<SkImage> *im = new sk_sp<SkImage>(SkImage::MakeFromBitmap(bm)); /// meta-smart.
            image.pixels.attach("sk-image", im, [im](var &) { delete im; });
        }
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = std::min(1.0, rect.w / isz.x);
        real scy = std::min(1.0, rect.h / isz.y);
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function
        pos.x = interp(rect.x, rect.x + rect.w - isz.x * sc, align->x);
        pos.y = interp(rect.y, rect.y + rect.h - isz.y * sc, align->y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler c;
        sk_canvas->scale(sc, sc);
        sk_canvas->drawImage(
            ((sk_sp<SkImage> *)image.pixels["sk-image"].n_value.vstar)->get(),
            SkScalar(0), SkScalar(0), SkSamplingOptions(c), &ps);
        sk_canvas->restore();
    }
    /// would be reasonable to have a rich() method

    /// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
    void text(str &text, rectd &rect, vec2 &align, vec2 &offset, bool ellip) {
        State   *s = (State *)top->b_state; // backend state (this)
        SkPaint ps = SkPaint(s->ps);
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkFont  &f = *top->font.handle();
        vec2   pos = { 0, 0 };
        str  stext;
        str *ptext = &text;
        text_metrics tm;
        if (ellip) {
            stext  = ellipsis(text, rect, tm);
            ptext  = &stext;
        } else
            tm     = measure(*ptext);
        auto    tb = SkTextBlob::MakeFromText(ptext->cstr(), ptext->size(), (const SkFont &)f, SkTextEncoding::kUTF8);
        pos.x = (align.x == Align::End)    ? rect.x + rect.w     - tm.w :
                (align.x == Align::Middle) ? rect.x + rect.w / 2 - tm.w / 2 : rect.x;
        pos.y = (align.y == Align::End)    ? rect.y + rect.h     - tm.h :
                (align.y == Align::Middle) ? rect.y + rect.h / 2 - tm.h / 2 - ((-tm.descent + tm.ascent) / 1.66) : rect.y;
        sk_canvas->drawTextBlob(
            tb, SkScalar(pos.x + offset.x),
                SkScalar(pos.y + offset.y), ps);
    }

    void clip(rectd &path) {
        assert(false);
    }

    void outline(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->strokePath(sk_path(path), ps);
    }

    void outline(graphics::shape &shape) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->strokePath(sk_path(shape), ps);
    }

    void Canvas::outline_sz(double sz) {
        data->outline_sz(sz);
    }

    void cap(graphics::cap &c) {
        top->ps.setStrokeCap(c == graphics::cap::Blunt ? SkPaint::kSquare_Cap :
                             c == graphics::cap::Round ? SkPaint::kRound_Cap  :
                                                         SkPaint::kButt_Cap);
    }

    void join(graphics::join &j) {
        top->ps.setStrokeJoin(j == Join::Bevel ? SkPaint::kBevel_Join :
                              j == Join::Round ? SkPaint::kRound_Join  :
                                                 SkPaint::kMiter_Join);
    }

    void translate(vec2d &tr) {
        sk_canvas->translate(SkScalar(tr.x), SkScalar(tr.y));
    }

    void scale(vec2d &sc) {
        sk_canvas->scale(SkScalar(sc.x), SkScalar(sc.y));
    }

    void rotate(double degs) {
        sk_canvas->rotate(degs);
    }

    void fill(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->drawRect(r, ps);
    }

    // we are to put everything in path.
    void fill(graphics::shape &path) {
        if (path.is_rect())
            return fill(path.rect);
        ///
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(sk_path(path), ps);
    }

    void clip(graphics::shape &path) {
        sk_canvas->clipPath(sk_path(path));
    }

    void gaussian(vec2 &sz, rectd &crop) {
        SkImageFilters::CropRect crect = { };
        if (crop) {
            SkRect rect = { SkScalar(crop.x),          SkScalar(crop.y),
                            SkScalar(crop.x + crop.w), SkScalar(crop.y + crop.h) };
            crect       = SkImageFilters::CropRect(rect);
        }
        sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sz.x, sz.y, nullptr, crect);
        host->state.blur = sz;
        top->ps.setImageFilter(std::move(filter));
    }
};


mx_implement(Canvas, mx);

u32 Canvas::width() { return data->sz.x; }
u32 Canvas::height() { return data->sz.y; }

void Canvas::resize(VkhImage image, int width, int height) {
    return data->resize(image, width, height);
}

void Canvas::font(ion::font f) {
    return data->font(f);
}
void Canvas::save() {
    return data->save();
}

void Canvas::clear() {
    return data->clear();
}

void Canvas::clear(rgbad c) {
    return data->clear(c);
}

void Canvas::color(rgbad c) {
    data->color(c);
}

void Canvas::opacity(double o) {
    data->opacity(o);
}

void Canvas::flush() {
    return data->flush();
}

void Canvas::restore() {
    return data->restore();
}
vec2i   Canvas::size() {
    return data->size();
}

text_metrics measure(str text) {
    return data->measure(text);
}

str     Canvas::ellipsis(str text, rectd rect, text_metrics &tm) {
    return data->ellipsis(text, rect, tm);
}

void    Canvas::image(image img, rectd rect, alignment align, vec2d offset) {
    return data->image(img, rect, align, offset);
}

void    Canvas::text(str text, rectd rect, vec2d align, vec2d offset, bool ellip) {
    return data->text(text, rect, align, offset, ellip);
}

void    Canvas::clip(rectd path) {
    return data->clip(path);
}

void Canvas::outline_sz(double sz) {
    data->outline_sz(sz);
}

void Canvas::outline(rectd rect) {
    return data->outline(rect);
}

void    Canvas::cap(graphics::cap c) {
    return data->cap(c);
}

void    Canvas::join(graphics::join j) {
    return data->join(j);
}

void    Canvas::translate(vec2d tr) {
    return data->translate(tr);
}

void    Canvas::scale(vec2d sc) {
    return data->scale(sc);
}

void    Canvas::rotate(double degs) {
    return data->rotate(degs);
}

void    Canvas::fill(rectd rect) {
    return data->fill(rect);
}

void    Canvas::fill(graphics::shape path) {
    return data->fill(path);
}

void    Canvas::clip(graphics::shape path) {
    return data->clip(path);
}

void    Canvas::gaussian(vec2d sz, rectd crop) {
    return data->gaussian(sz, crop);
}