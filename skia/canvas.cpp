
#include <mx/mx.hpp>
#include <media/image.hpp>
#include <trinity/trinity.hpp>
#include <skia/system.hpp>

#define CANVAS_IMPL
#include <skia/canvas.hpp>

using namespace ion;

namespace ion {

inline SkColor sk_color(rgba c) {
    rgba8 i = {
        u8(math::round(c.x * 255)), u8(math::round(c.y * 255)),
        u8(math::round(c.z * 255)), u8(math::round(c.w * 255)) };
    auto sk = SkColor(uint32_t(i.z)        | (uint32_t(i.y) << 8) |
                     (uint32_t(i.x) << 16) | (uint32_t(i.w) << 24));
    return sk;
}

inline SkColor sk_color(rgba8 c) {
    auto sk = SkColor(uint32_t(c.z)        | (uint32_t(c.y) << 8) |
                     (uint32_t(c.x) << 16) | (uint32_t(c.w) << 24));
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
    str canvas_id;
    std::unique_ptr<skgpu::graphite::Context>  fGraphiteContext;
    std::unique_ptr<skgpu::graphite::Recorder> fGraphiteRecorder;
    //DisplayParams     params;
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
        m44f        m;
        rgba        color;
        ion::outline clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
        m44f        model, view, proj;
    };

    ~ICanvas() {
        if (texture.device())
            texture.cleanup_resize(canvas_id);
    }

    state *top = null;
    doubly stack;

    void outline_sz(double sz) {
        top->outline_sz = sz;
    }

    void color(rgba &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    void canvas_new_texture(int width, int height) {
        dpi_scale = 1.0f;
        identity();
    }

    SkPath *sk_path(ion::outline &sh) {
        ion::outline::sdata &shape = sh.ref<ion::outline::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *(SkPath*)shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rect)) {
                rect &m = sh->bounds;
                SkRect r = SkRect {
                    float(m.x), float(m.y), float(m.x + m.w), float(m.y + m.h)
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded)) {
                Rounded::rdata &m = sh->bounds.mem->ref<Rounded::rdata>();
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
                ion::outline::sdata &m = *sh.data;
                for (mx &o:m.ops.elements<mx>()) {
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
        state &s = stack->push<state>();
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

    m44f get_matrix() {
        SkM44 skm = sk_canvas->getLocalToDevice();
        m44f res(0.0);
        skm.getColMajor(reinterpret_cast<SkScalar*>(&res));
        return res;
    }


    void    clear()        { sk_canvas->clear(SK_ColorTRANSPARENT); }
    void    clear(rgba c) { sk_canvas->clear(sk_color(c)); }

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
        top = stack->len() ? &stack->last<state>() : null;
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
            fabs(mx.fAscent) + fabs(mx.fDescent),
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
    str ellipsis(str &text, ion::rect &rect, text_metrics &tm) {
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


    void image(ion::SVG &image, ion::rect &rect, alignment &align, vec2d &offset) {
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

    void image(ion::image &image, ion::rect &rect, alignment &align, vec2d &offset, bool attach_tx) {
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
    void text(str &text, ion::rect &rect, alignment &align, vec2d &offset, bool ellip, ion::rect *placement) {
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

    void clip(ion::rect &rect) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->clipRect(r);
    }

    void outline(Array<vec2f> &line, bool is_fill = false) {
        SkPaint ps = SkPaint(top->ps);
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(is_fill ? 0 : top->outline_sz);
        ps.setStroke(!is_fill);
        vec2f *a = null;
        SkPath path;
        for (vec2f &b: line) {
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

    void projection(m44f      &m, m44f      &v, m44f      &p) {
        top->model      = m;
        top->view       = v;
        top->proj       = p;
    }

    void outline(Array<vec3f> &v3, bool is_fill = false) {
        vec2f sz = { float(this->sz.x / this->dpi_scale.x), float(this->sz.y / this->dpi_scale.y) };
        Array<vec2f> projected { v3.len() };
        for (vec3f &vertex: v3) {
            vec4f cs  = top->proj * top->view * top->model * vec4f(vertex.x, vertex.y, vertex.z, 1.0f);
            vec3f ndc = (cs / cs.w).xyz();
            float screenX = ((ndc.x + 1) / 2.0) * sz.x;
            float screenY = ((1 - ndc.y) / 2.0) * sz.y;
            vec2f  v2 = { screenX, screenY };
            projected    += v2;
        }
        outline(projected, is_fill);
    }

    void line(vec3f &a, vec3f &b) {
        Array<vec3f> ab { size_t(2) };
        ab.push(a);
        ab.push(b);
        outline(ab);
    }

    void arc(vec3f position, real radius, real startAngle, real endAngle, bool is_fill = false) {
        const int segments = 36;
        ion::Array<vec3f> arcPoints { size_t(segments) };
        float angleStep = (endAngle - startAngle) / segments;

        for (int i = 0; i <= segments; ++i) {
            float     angle = radians(startAngle + angleStep * i);
            vec3f point;
            point.x = position.x + radius * cos(angle);
            point.y = position.y;
            point.z = position.z + radius * sin(angle);
            vec4f viewSpacePoint = top->view * vec4f(point, 1.0f);
            vec3f clippingSp     = vec3f(viewSpacePoint);
            arcPoints += clippingSp;
        }
        arcPoints.set_size(segments);
        outline(arcPoints, is_fill);
    }

    void outline(ion::rect &rect) {
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

    void outline(ion::outline &shape) {
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

    void cap(ion::cap &c) {
        top->ps.setStrokeCap(c == ion::cap::blunt ? SkPaint::kSquare_Cap :
                             c == ion::cap::round ? SkPaint::kRound_Cap  :
                                               SkPaint::kButt_Cap);
    }

    void join(join &j) {
        top->ps.setStrokeJoin(j == ion::join::bevel ? SkPaint::kBevel_Join :
                              j == ion::join::round ? SkPaint::kRound_Join  :
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

    void draw_rect(ion::rect &rect, SkPaint &ps) {
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

    void fill(ion::rect &rect) {
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
    void fill(ion::outline &path) {
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

    void clip(ion::outline &path) {
        sk_canvas->clipPath(*sk_path(path));
    }

    void gaussian(vec2d &sz, ion::rect &crop) {
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
};

mx_implement(Canvas, mx, ICanvas);

/// create Canvas with a texture; register resizing updates so we can update without recreating the object
Canvas::Canvas(Texture texture) : Canvas() {
    static int n_canvas = 0;
    n_canvas++;
    
    data->canvas_id = fmt {"canvas{0}", {n_canvas}};
    data->texture = texture;
    ICanvas *icanvas = data;

    auto update = [data=icanvas]() {

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
        Device device = data->texture.device();
        wgpu::Device *idevice = (wgpu::Device*)device.handle();
        backendContext.fDevice = *idevice;
        backendContext.fQueue = idevice->GetQueue();

        data->fGraphiteContext = null;
        data->fGraphiteRecorder = null;

        data->fGraphiteContext = skgpu::graphite::ContextFactory::MakeDawn(
                backendContext, params.fGraphiteContextOptions.fOptions);

        skgpu::graphite::RecorderOptions opts;
        data->fGraphiteRecorder = data->fGraphiteContext->makeRecorder(opts);

        wgpu::Texture *itexture = (wgpu::Texture*)data->texture.handle();

        skgpu::graphite::BackendTexture backend_texture(itexture->Get());
        data->sk_surf = SkSurfaces::WrapBackendTexture(
            data->fGraphiteRecorder.get(),
            backend_texture,
            kBGRA_8888_SkColorType,
            params.fColorSpace,
            &params.fSurfaceProps);

        float xscale = 1.0f, yscale = 1.0f;
        device.get_dpi(&xscale, &yscale);

        data->dpi_scale.x = xscale;
        data->dpi_scale.y = yscale;
        data->sz          = data->texture.size(); // use virtual functions to obtain the virtual size
        data->sk_canvas   = data->sk_surf->getCanvas();
    };

    update();

    data->save();

    // this is slower than reconstructing canvas, somehow...
    //texture.on_resize(data->canvas_id, [update](vec2i sz) {
    //    update();
    //});
}


u32 Canvas::get_virtual_width()  { return data->sz.x / data->dpi_scale.x; }
u32 Canvas::get_virtual_height() { return data->sz.y / data->dpi_scale.y; }

//void Canvas::canvas_resize(VkhImage image, int width, int height) {
//    return data->canvas_resize(image, width, height);
//}

void Canvas::font(ion::font f)              { data->font(f); }
void Canvas::save()                         { data->save(); }
void Canvas::clear()                        { data->clear(); }
void Canvas::clear(rgba c)                 { data->clear(c); }
void Canvas::color(rgba c)                 { data->color(c); }
void Canvas::opacity(double o)              { data->opacity(o); }
void Canvas::flush()                        { data->flush(); }
void Canvas::restore()                      { data->restore(); }
vec2i Canvas::size()                        { return data->size(); }
void Canvas::clip(ion::rect path)           { data->clip(path); }
void Canvas::outline_sz(double sz)          { data->outline_sz(sz); }
void Canvas::outline(ion::rect rect)        { data->outline(rect); }
void Canvas::cap(ion::cap c)                { data->cap(c); }
void Canvas::join(ion::join j)              { data->join(j); }
void Canvas::translate(vec2d tr)            { data->translate(tr); }
void Canvas::scale(vec2d sc)                { data->scale(sc); }
void Canvas::rotate(double degs)            { data->rotate(degs); }
void Canvas::outline(Array<vec3f> v3)   { data->outline(v3); }
void Canvas::outline(Array<vec2f> line) { data->outline(line); }
void Canvas::fill(ion::rect rect)               { data->fill(rect); }
void Canvas::fill(ion::outline path)     { data->fill(path); }
void Canvas::clip(ion::outline path)     { data->clip(path); }
void Canvas::gaussian(vec2d sz, ion::rect crop) { data->gaussian(sz, crop); }
void Canvas::line(vec3f &a, vec3f &b) { data->line(a, b); }
void Canvas::projection(m44f      &m, m44f      &v, m44f      &p) { return data->projection(m, v, p); }

void Canvas::arc(vec3f pos, real radius, real startAngle, real endAngle, bool is_fill) {
    return data->arc(pos, radius, startAngle, endAngle, is_fill);
}

text_metrics Canvas::measure(str text) {
    return data->measure(text);
}

double Canvas::measure_advance(char *text, size_t len) {
    return data->measure_advance(text, len);
}

str Canvas::ellipsis(str text, ion::rect rect, text_metrics &tm) {
    return data->ellipsis(text, rect, tm);
}

void Canvas::image(ion::SVG img, ion::rect rect, alignment align, vec2d offset) {
    return data->image(img, rect, align, offset);
}

void Canvas::image(ion::image img, ion::rect rect, alignment align, vec2d offset, bool attach_tx) {
    return data->image(img, rect, align, offset, attach_tx);
}

void Canvas::text(str text, ion::rect rect, alignment align, vec2d offset, bool ellip, ion::rect *placement) {
    return data->text(text, rect, align, offset, ellip, placement);
}

SVG::M::operator bool() {
    return w > 0;
}

vec2i SVG::sz() { return { data->w, data->h }; }

void SVG::set_props(EProps &eprops) {
    // iterate through fields in map (->eprops)
    for (field &f: eprops->eprops.fields()) {
        str id_attr(f.key);
        str value(f.value); /// call operator str()
        Array<str> ida = id_attr.split(".");
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

Array<double> font::advances(Canvas& canvas, str text) {
    num l = text.len();
    Array<double> res(l+1, l+1);
    ///
    for (num i = 0; i <= l; i++) {
        str    s   = text.mid(0, i);
        double adv = canvas.measure_advance(s.data, i);
        res[i]     = adv;
    }
    return res;
}

}