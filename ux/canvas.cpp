
#include <mx/mx.hpp>
#include <media/image.hpp>
#include <dawn/dawn.hpp>

#include <dawn/webgpu_cpp.h>
#include <dawn/dawn_proc.h>
#include <webgpu/webgpu_glfw.h>
#include <dawn/native/DawnNative.h>
#include <dawn/utils/ComboRenderPipelineDescriptor.h>
#include <dawn/samples/SampleUtils.h>
#include <dawn/utils/SystemUtils.h>
#include <dawn/utils/WGPUHelpers.h>
#include <dawn/common/Log.h>
#include <dawn/common/Platform.h>
#include <dawn/common/SystemUtils.h>

#include <gpu/graphite/dawn/DawnUtils.h>
#include <gpu/graphite/dawn/DawnTypes.h>
#include <gpu/graphite/BackendTexture.h>
#include <gpu/graphite/dawn/DawnBackendContext.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Surface.h>
#include <gpu/graphite/Surface_Graphite.h>
#include <core/SkPath.h>
#include <core/SkFont.h>
#include <core/SkRRect.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColorSpace.h>
#include <core/SkSurface.h>
#include <core/SkFontMgr.h>
#include <ports/SkFontMgr_empty.h>
#include <core/SkFontMetrics.h>
#include <core/SkPathMeasure.h>
#include <core/SkPathUtils.h>
#include <core/SkStroke.h>
#include <core/SkTextBlob.h>
#include <core/SkStream.h>
#include <core/SkAlphaType.h>
#include <core/SkColor.h>
#include <core/SkColorType.h>
#include <core/SkImageInfo.h>
#include <core/SkRefCnt.h>
#include <core/SkTypes.h>
#include <effects/SkGradientShader.h>
#include <effects/SkImageFilters.h>
#include <effects/SkDashPathEffect.h>
#include <modules/svg/include/SkSVGDOM.h>
#include <modules/svg/include/SkSVGNode.h>
#include <tools/GpuToolUtils.h>
#include <tools/window/WindowContext.h>
#include <tools/viewer/SimpleStrokerSlide.h>

#define CANVAS_IMPL
#include <ux/canvas.hpp>

using namespace ion;

namespace ion {

inline SkColor sk_color(rgbad c) {
    rgba8 i = {
        u8(math::round(c.r * 255)), u8(math::round(c.g * 255)),
        u8(math::round(c.b * 255)), u8(math::round(c.a * 255)) };
    auto sk = SkColor(uint32_t(i.b)        | (uint32_t(i.g) << 8) |
                     (uint32_t(i.r) << 16) | (uint32_t(i.a) << 24));
    return sk;
}

inline SkColor sk_color(rgba8 c) {
    auto sk = SkColor(uint32_t(c.b)        | (uint32_t(c.g) << 8) |
                     (uint32_t(c.r) << 16) | (uint32_t(c.a) << 24));
    return sk;
}

SVG::SVG(path p) : SVG() {
    SkStream* stream = new SkFILEStream((symbol)p.cs());
    data->svg_dom = new sk_sp<SkSVGDOM>(SkSVGDOM::MakeFromStream(*stream));
    SkSize size = (*data->svg_dom)->containerSize();
    data->w = size.fWidth;
    data->h = size.fHeight;
    delete stream;
}

SVG::SVG(cstr p) : SVG(path(p)) { }

struct ICanvas {
    std::unique_ptr<skgpu::graphite::Context>  fGraphiteContext;
    std::unique_ptr<skgpu::graphite::Recorder> fGraphiteRecorder;
    //DisplayParams     params;
    bool           use_hidpi;
    Texture          texture;
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    vec2i             sz_raw;
    vec2d          dpi_scale = { 1, 1 };

    struct state {
        ion::image  img;
        double      outline_sz = 0.0;
        double      font_scale = 1.0;
        double      opacity    = 1.0;
        m44d        m;
        rgbad       color;
        graphics::shape clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
        glm::mat4   model, view, proj;
    };

    state *top = null;
    doubly<state> stack;

    void outline_sz(double sz) {
        top->outline_sz = sz;
    }

    void color(rgbad &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    void canvas_new_texture(int width, int height) {
        dpi_scale = 1.0f;
        identity();
    }

    SkPath *sk_path(graphics::shape &sh) {
        graphics::shape::sdata &shape = sh.ref<graphics::shape::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *(SkPath*)shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rectd)) {
                rectd &m = sh->bounds;
                SkRect r = SkRect {
                    float(m.x), float(m.y), float(m.x + m.w), float(m.y + m.h)
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded<double>)) {
                Rounded<double>::rdata &m = sh->bounds.mem->ref<Rounded<double>::rdata>();
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
                graphics::shape::sdata &m = *sh.data;
                for (mx &o:m.ops) {
                    type_t t = o.type();
                    if (t == typeof(Movement)) {
                        Movement m(o);
                        p.moveTo(m->x, m->y);
                    } else if (t == typeof(Line)) {
                        Line l(o);
                        p.lineTo(l->origin.x, l->origin.y); /// todo: origin and to are swapped, i believe
                    }
                }
                p.close();
            }
        }

        /// issue here is reading the data, which may not be 'sdata', but Rect, Rounded
        /// so the case below is 
        if (bool(shape.sk_offset) && shape.cache_offset == shape.offset)
            return (SkPath*)shape.sk_offset;
        ///
        if (!std::isnan(shape.offset) && shape.offset != 0) {
            assert(shape.sk_path); /// must have an actual series of shape operations in skia
            ///
            delete (SkPath*)shape.sk_offset;

            SkPath *o = (SkPath*)shape.sk_offset;
            shape.cache_offset = shape.offset;
            ///
            SkPath  fpath;
            SkPaint cp = SkPaint(top->ps);
            cp.setStyle(SkPaint::kStroke_Style);
            cp.setStrokeWidth(std::abs(shape.offset) * 2);
            cp.setStrokeJoin(SkPaint::kRound_Join);

            SkPathStroker2 stroker;
            SkPath offset_path = stroker.getFillPath(*(SkPath*)shape.sk_path, cp);
            shape.sk_offset = new SkPath(offset_path);
            
            auto vrbs = ((SkPath*)shape.sk_path)->countVerbs();
            auto pnts = ((SkPath*)shape.sk_path)->countPoints();
            std::cout << "sk_path = " << (void *)shape.sk_path << ", pointer = " << (void *)this << " verbs = " << vrbs << ", points = " << pnts << "\n";
            ///
            if (shape.offset < 0) {
                o->reverseAddPath(fpath);
                o->setFillType(SkPathFillType::kWinding);
            } else
                o->addPath(fpath);
            ///
            return o;
        }
        return (SkPath*)shape.sk_path;
    }

    void font(ion::font &f) { 
        top->font = f;
    }
    
    void save() {
        state &s = stack->push();
        if (top) {
            s = *top;
        } else {
            s.ps = SkPaint { };
        }
        sk_canvas->save();
        top = &s;
    }

    void identity() {
        sk_canvas->resetMatrix();
        sk_canvas->scale(dpi_scale.x, dpi_scale.y);
    }

    void set_matrix() {
    }

    m44d get_matrix() {
        SkM44 skm = sk_canvas->getLocalToDevice();
        m44d res(0.0);
        return res;
    }


    void    clear()        { sk_canvas->clear(SK_ColorTRANSPARENT); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        /// this is what i wasnt doing:
        std::unique_ptr<skgpu::graphite::Recording> recording = fGraphiteRecorder->snap();
        if (recording) {
            skgpu::graphite::InsertRecordingInfo info;
            info.fRecording = recording.get();
            fGraphiteContext->insertRecording(info);
            fGraphiteContext->submit(skgpu::graphite::SyncToCpu::kNo);
        }
    }

    void  restore() {
        stack->pop();
        top = stack->len() ? &stack->last() : null;
        sk_canvas->restore();
    }

    vec2i size() { return sz; }

    /// console would just think of everything in char units. like it is.
    /// measuring text would just be its length, line height 1.
    text_metrics measure(str &text) {
        SkFontMetrics mx;
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text.cs(), text.len(), SkTextEncoding::kUTF8);
        auto          lh = font.getMetrics(&mx);

        return text_metrics {
            adv,
            abs(mx.fAscent) + abs(mx.fDescent),
            mx.fAscent,
            mx.fDescent,
            lh,
            mx.fCapHeight
        };
    }

    double measure_advance(char *text, size_t len) {
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text, len, SkTextEncoding::kUTF8);
        return (double)adv;
    }

    /// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
    /// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
    /// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone
    str ellipsis(str &text, rectd &rect, text_metrics &tm) {
        const str el = "...";
        str       cur, *p = &text;
        int       trim = p->len();
        tm             = measure((str &)el);
        
        if (tm.w >= rect.w)
            trim = 0;
        else
            for (;;) {
                tm = measure(*p);
                if (tm.w <= rect.w || trim == 0)
                    break;
                if (tm.w > rect.w && trim >= 1) {
                    cur = text.mid(0, --trim) + el;
                    p   = &cur;
                }
            }
        return (trim == 0) ? "" : (p == &text) ? text : cur;
    }


    void image(ion::SVG &image, rectd &rect, alignment &align, vec2d &offset) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function (again)
        pos.x = mix(rect.x, rect.x + rect.w - isz.x * sc, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - isz.y * sc, align.y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        sk_canvas->scale(sc, sc);
        image.render(sk_canvas, rect.w, rect.h);
        sk_canvas->restore();
    }

    void image(ion::image &image, rectd &rect, alignment &align, vec2d &offset, bool attach_tx) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// cache SkImage using memory attachments
        attachment *att = image.mem->find_attachment("sk-image");
        sk_sp<SkImage> *im;
        if (!att) {
            SkBitmap bm;
            rgba8          *px = image.pixels();
            //memset(px, 255, 640 * 360 * 4);
            SkImageInfo   info = SkImageInfo::Make(isz.x, isz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
            sz_t        stride = image.stride() * sizeof(rgba8);
            bm.installPixels(info, px, stride);
            sk_sp<SkImage> bm_image = bm.asImage();
            im = new sk_sp<SkImage>(bm_image);
            if (attach_tx)
                att = image.mem->attach("sk-image", im, [im]() { delete im; });
        }
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        
        if (!align.is_default) {
            scx   = scy = (scy > scx) ? scx : scy;
            /// no enums were harmed during the making of this function
            pos.x = mix(rect.x, rect.x + rect.w - isz.x * scx, align.x);
            pos.y = mix(rect.y, rect.y + rect.h - isz.y * scy, align.y);
            
        } else {
            /// if alignment is default state, scale directly by bounds w/h, position at bounds x/y
            pos.x = rect.x;
            pos.y = rect.y;
        }
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler cubicResampler { 1.0f/3, 1.0f/3 };
        SkSamplingOptions samplingOptions(cubicResampler);

        sk_canvas->scale(scx, scy);
        SkImage *img = im->get();
        sk_canvas->drawImage(img, SkScalar(0), SkScalar(0), samplingOptions);
        sk_canvas->restore();

        if (!attach_tx) {
            flush();
        }
    }

    /// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
    void text(str &text, rectd &rect, alignment &align, vec2d &offset, bool ellip, rectd *placement) {
        SkPaint ps = SkPaint(top->ps);
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkFont  &f = font_handle(top->font);
        vec2d  pos = { 0, 0 };
        str  stext;
        str *ptext = &text;
        text_metrics tm;
        if (ellip) {
            stext  = ellipsis(text, rect, tm);
            ptext  = &stext;
        } else
            tm     = measure(*ptext);
        auto    tb = SkTextBlob::MakeFromText(ptext->cs(), ptext->len(), (const SkFont &)f, SkTextEncoding::kUTF8);
        pos.x = mix(rect.x, rect.x + rect.w - tm.w, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - tm.h, align.y);
        double skia_y_offset = (tm.descent + -tm.ascent) / 1.5;
        /// set placement rect if given (last paint is what was rendered)
        if (placement) {
            placement->x = pos.x + offset.x;
            placement->y = pos.y + offset.y;
            placement->w = tm.w;
            placement->h = tm.h;
        }
        sk_canvas->drawTextBlob(
            tb, SkScalar(pos.x + offset.x),
                SkScalar(pos.y + offset.y + skia_y_offset), ps);
    }

    void clip(rectd &rect) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->clipRect(r);
    }

    void outline(array<glm::vec2> &line, bool is_fill = false) {
        SkPaint ps = SkPaint(top->ps);
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(is_fill ? 0 : top->outline_sz);
        ps.setStroke(!is_fill);
        glm::vec2 *a = null;
        SkPath path;
        for (glm::vec2 &b: line) {
            SkPoint bp = { b.x, b.y };
            if (a) {
                path.lineTo(bp);
            } else {
                path.moveTo(bp);
            }
            a = &b;
        }
        sk_canvas->drawPath(path, ps);
    }

    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
        top->model      = m;
        top->view       = v;
        top->proj       = p;
    }

    void outline(array<glm::vec3> &v3, bool is_fill = false) {
        glm::vec2 sz = { this->sz.x / this->dpi_scale.x, this->sz.y / this->dpi_scale.y };
        array<glm::vec2> projected { v3.len() };
        for (glm::vec3 &vertex: v3) {
            glm::vec4 cs  = top->proj * top->view * top->model * glm::vec4(vertex, 1.0f);
            glm::vec3 ndc = cs / cs.w;
            float screenX = ((ndc.x + 1) / 2.0) * sz.x;
            float screenY = ((1 - ndc.y) / 2.0) * sz.y;
            glm::vec2  v2 = { screenX, screenY };
            projected    += v2;
        }
        outline(projected, is_fill);
    }

    void line(glm::vec3 &a, glm::vec3 &b) {
        array<glm::vec3> ab { size_t(2) };
        ab.push(a);
        ab.push(b);
        outline(ab);
    }

    void arc(glm::vec3 position, real radius, real startAngle, real endAngle, bool is_fill = false) {
        const int segments = 36;
        ion::array<glm::vec3> arcPoints { size_t(segments) };
        float angleStep = (endAngle - startAngle) / segments;

        for (int i = 0; i <= segments; ++i) {
            float     angle = glm::radians(startAngle + angleStep * i);
            glm::vec3 point;
            point.x = position.x + radius * cos(angle);
            point.y = position.y;
            point.z = position.z + radius * sin(angle);
            glm::vec4 viewSpacePoint = top->view * glm::vec4(point, 1.0f);
            glm::vec3 clippingSp     = glm::vec3(viewSpacePoint);
            arcPoints += clippingSp;
        }
        arcPoints.set_size(segments);
        outline(arcPoints, is_fill);
    }

    void outline(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        draw_rect(rect, ps);
    }

    void outline(graphics::shape &shape) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!shape.is_rect());
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(shape), ps);
    }

    void cap(graphics::cap &c) {
        top->ps.setStrokeCap(c == graphics::cap::blunt ? SkPaint::kSquare_Cap :
                             c == graphics::cap::round ? SkPaint::kRound_Cap  :
                                                         SkPaint::kButt_Cap);
    }

    void join(graphics::join &j) {
        top->ps.setStrokeJoin(j == graphics::join::bevel ? SkPaint::kBevel_Join :
                              j == graphics::join::round ? SkPaint::kRound_Join  :
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

    void draw_rect(rectd &rect, SkPaint &ps) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        
        if (rect.rounded) {
            SkRRect rr;
            SkVector corners[4] = {
                { float(rect.r_tl.x), float(rect.r_tl.y) },
                { float(rect.r_tr.x), float(rect.r_tr.y) },
                { float(rect.r_br.x), float(rect.r_br.y) },
                { float(rect.r_bl.x), float(rect.r_bl.y) }
            };
            rr.setRectRadii(r, corners);
            sk_canvas->drawRRect(rr, ps);
        } else {
            sk_canvas->drawRect(r, ps);
        }
    }

    void fill(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setColor(sk_color(top->color));
        ps.setAntiAlias(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));

        draw_rect(rect, ps);
    }

    // we are to put everything in path.
    void fill(graphics::shape &path) {
        if (path.is_rect())
            return fill(path->bounds);
        ///
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(path), ps);
    }

    void clip(graphics::shape &path) {
        sk_canvas->clipPath(*sk_path(path));
    }

    void gaussian(vec2d &sz, rectd &crop) {
        SkImageFilters::CropRect crect = { };
        if (crop) {
            SkRect rect = { SkScalar(crop.x),          SkScalar(crop.y),
                            SkScalar(crop.x + crop.w), SkScalar(crop.y + crop.h) };
            crect       = SkImageFilters::CropRect(rect);
        }
        sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sz.x, sz.y, nullptr, crect);
        top->blur = sz;
        top->ps.setImageFilter(std::move(filter));
    }

    SkFont &font_handle(ion::font &font) {
        if (!font->sk_font) {
            /// dpi scaling is supported at the SkTypeface level, just add the scale x/y
            path p = font.get_path();

            SkFILEStream input((symbol)p.cs());
            assert (input.isValid());

            sk_sp<SkFontMgr>    mgr = SkFontMgr_New_Custom_Empty();
            sk_sp<SkData> font_data = SkData::MakeFromStream(&input, input.getLength());
            sk_sp<SkTypeface>     t = mgr->makeFromData(font_data);

            font->sk_font = new SkFont(t);
            ((SkFont*)font->sk_font)->setSize(font->sz);
        }
        return *(SkFont*)font->sk_font;
    }
    type_register(ICanvas);
};

mx_implement(Canvas, mx, ICanvas);

/// create Canvas with a texture
Canvas::Canvas(Device device, Texture texture, bool use_hidpi) : Canvas() {
    data->texture = texture;

    skwindow::DisplayParams params { };
    params.fColorType = kRGBA_8888_SkColorType;
    params.fColorSpace = SkColorSpace::MakeSRGB();
    params.fMSAASampleCount = 0;
    params.fGrContextOptions = GrContextOptions();
    params.fSurfaceProps = SkSurfaceProps(0, kBGR_H_SkPixelGeometry);
    params.fDisableVsync = false;
    params.fDelayDrawableAcquisition = false;
    params.fEnableBinaryArchive = false;
    params.fCreateProtectedNativeBackend = false;
    params.fGraphiteContextOptions.fPriv.fStoreContextRefInRecorder = true; // Needed to make synchronous readPixels work:

    skgpu::graphite::DawnBackendContext backendContext;
    wgpu::Device *idevice = (wgpu::Device*)device.handle();
    backendContext.fDevice = *idevice;
    backendContext.fQueue = idevice->GetQueue();

    data->fGraphiteContext = skgpu::graphite::ContextFactory::MakeDawn(
            backendContext, params.fGraphiteContextOptions.fOptions);
    data->fGraphiteRecorder = data->fGraphiteContext->makeRecorder(ToolUtils::CreateTestingRecorderOptions());

    wgpu::Texture *itexture = (wgpu::Texture*)texture.handle();

    skgpu::graphite::BackendTexture backend_texture(itexture->Get());
    data->sk_surf = SkSurfaces::WrapBackendTexture(
        data->fGraphiteRecorder.get(),
        backend_texture,
        kBGRA_8888_SkColorType,
        params.fColorSpace,
        &params.fSurfaceProps);

    float xscale = 1.0f, yscale = 1.0f;
    device.get_dpi(&xscale, &yscale);
    if (use_hidpi) {
        xscale = math::max(2.0f, xscale);
        yscale = math::max(2.0f, yscale);
    }

    data->dpi_scale.x = xscale;
    data->dpi_scale.y = yscale;
    data->sz          = texture.size(); // use virtual functions to obtain the virtual size
    data->sk_canvas   = data->sk_surf->getCanvas();
    data->use_hidpi   = use_hidpi;
    data->save();
}


u32 Canvas::get_virtual_width()  { return data->sz.x / data->dpi_scale.x; }
u32 Canvas::get_virtual_height() { return data->sz.y / data->dpi_scale.y; }

//void Canvas::canvas_resize(VkhImage image, int width, int height) {
//    return data->canvas_resize(image, width, height);
//}

void Canvas::font(ion::font f)              { data->font(f); }
void Canvas::save()                         { data->save(); }
void Canvas::clear()                        { data->clear(); }
void Canvas::clear(rgbad c)                 { data->clear(c); }
void Canvas::color(rgbad c)                 { data->color(c); }
void Canvas::opacity(double o)              { data->opacity(o); }
void Canvas::flush()                        { data->flush(); }
void Canvas::restore()                      { data->restore(); }
vec2i Canvas::size()                        { return data->size(); }
void Canvas::clip(rectd path)               { data->clip(path); }
void Canvas::outline_sz(double sz)          { data->outline_sz(sz); }
void Canvas::outline(rectd rect)            { data->outline(rect); }
void Canvas::cap(graphics::cap c)           { data->cap(c); }
void Canvas::join(graphics::join j)         { data->join(j); }
void Canvas::translate(vec2d tr)            { data->translate(tr); }
void Canvas::scale(vec2d sc)                { data->scale(sc); }
void Canvas::rotate(double degs)            { data->rotate(degs); }
void Canvas::outline(array<glm::vec3> v3)   { data->outline(v3); }
void Canvas::outline(array<glm::vec2> line) { data->outline(line); }
void Canvas::fill(rectd rect)               { data->fill(rect); }
void Canvas::fill(graphics::shape path)     { data->fill(path); }
void Canvas::clip(graphics::shape path)     { data->clip(path); }
void Canvas::gaussian(vec2d sz, rectd crop) { data->gaussian(sz, crop); }
void Canvas::line(glm::vec3 &a, glm::vec3 &b) { data->line(a, b); }
void Canvas::projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) { return data->projection(m, v, p); }

void Canvas::arc(glm::vec3 pos, real radius, real startAngle, real endAngle, bool is_fill) {
    return data->arc(pos, radius, startAngle, endAngle, is_fill);
}

text_metrics Canvas::measure(str text) {
    return data->measure(text);
}

double Canvas::measure_advance(char *text, size_t len) {
    return data->measure_advance(text, len);
}

str Canvas::ellipsis(str text, rectd rect, text_metrics &tm) {
    return data->ellipsis(text, rect, tm);
}

void Canvas::image(ion::SVG img, rectd rect, alignment align, vec2d offset) {
    return data->image(img, rect, align, offset);
}

void Canvas::image(ion::image img, rectd rect, alignment align, vec2d offset, bool attach_tx) {
    return data->image(img, rect, align, offset, attach_tx);
}

void Canvas::text(str text, rectd rect, alignment align, vec2d offset, bool ellip, rectd *placement) {
    return data->text(text, rect, align, offset, ellip, placement);
}

SVG::M::operator bool() {
    return w > 0;
}

vec2i SVG::sz() { return { data->w, data->h }; }

void SVG::set_props(EProps &eprops) {
    // iterate through fields in map (->eprops)
    for (field<EStr> &f: eprops->eprops) {
        str id_attr = f.key.hold();
        str value   = str(f.value); /// call operator str()
        array<str> ida = id_attr.split(".");
        assert(ida.len() == 2);

        symbol id   = ida[0].cs();
        symbol attr = ida[1].cs();
        symbol val  = value.cs();
        
        sk_sp<SkSVGNode>* n = (*data->svg_dom)->findNodeById(id);
        assert(n);
        
        bool set = (*n)->setAttribute(attr, val);
        assert(set);
    }
}

void SVG::render(SkCanvas *sk_canvas, int w, int h) {
    if (w == -1) w = data->w;
    if (h == -1) h = data->h;
    if (data->rw != w || data->rh != h) {
        data->rw  = w;
        data->rh  = h;
        (*data->svg_dom)->setContainerSize(
            SkSize::Make(data->rw, data->rh));
    }
    (*data->svg_dom)->render(sk_canvas);
}

array<double> font::advances(Canvas& canvas, str text) {
    num l = text.len();
    array<double> res(l+1, l+1);
    ///
    for (num i = 0; i <= l; i++) {
        str    s   = text.mid(0, i);
        double adv = canvas.measure_advance(s.data, i);
        res[i]     = adv;
    }
    return res;
}

}