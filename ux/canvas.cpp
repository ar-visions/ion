#include <skia/core/SkImage.h>

#define  SK_VULKAN
#include <skia/gpu/vk/GrVkBackendContext.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/GrDirectContext.h>
#include <skia/gpu/vk/VulkanExtensions.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkSurface.h>
#include <skia/core/SkFontMgr.h>
#include <skia/core/SkFontMetrics.h>
#include <skia/core/SkPathMeasure.h>
#include <skia/core/SkPathUtils.h>
#include <skia/utils/SkParsePath.h>
#include <skia/core/SkTextBlob.h>
#include <skia/effects/SkGradientShader.h>
#include <skia/effects/SkImageFilters.h>
#include <skia/effects/SkDashPathEffect.h>
#include <skia/core/SkStream.h>
#include <skia/modules/svg/include/SkSVGDOM.h>
#include <skia/core/SkAlphaType.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkColorType.h>
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkRefCnt.h>
#include <skia/core/SkTypes.h>
#include <skia/gpu/GrDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/vk/GrVkBackendContext.h>
#include <skia/gpu/vk/VulkanExtensions.h>
//#include <skia/tools/gpu/vk/VkTestUtils.h>

#include <ux/ux.hpp>
#include <ux/canvas.hpp>
#include <media/media.hpp>
#include <vk/vk.hpp>
#include <vk/vkh_device.h>
#include <vk/vkh_image.h>
#include <vk/vkh_presenter.h>

using namespace ion;

static SkPoint rotate90(const SkPoint& p) { return {p.fY, -p.fX}; }
static SkPoint rotate180(const SkPoint& p) { return p * -1; }
static SkPoint setLength(SkPoint p, float len) {
    if (!p.setLength(len)) {
        SkDebugf("Failed to set point length\n");
    }
    return p;
}
static bool isClockwise(const SkPoint& a, const SkPoint& b) { return a.cross(b) > 0; }

/** Helper class for constructing paths, with undo support */
class PathRecorder {
public:
    SkPath getPath() const {
        return SkPath::Make(fPoints.data(), fPoints.size(), fVerbs.data(), fVerbs.size(), nullptr,
                            0, SkPathFillType::kWinding);
    }

    void moveTo(SkPoint p) {
        fVerbs.push_back(SkPath::kMove_Verb);
        fPoints.push_back(p);
    }

    void lineTo(SkPoint p) {
        fVerbs.push_back(SkPath::kLine_Verb);
        fPoints.push_back(p);
    }

    void close() { fVerbs.push_back(SkPath::kClose_Verb); }

    void rewind() {
        fVerbs.clear();
        fPoints.clear();
    }

    int countPoints() const { return fPoints.size(); }

    int countVerbs() const { return fVerbs.size(); }

    bool getLastPt(SkPoint* lastPt) const {
        if (fPoints.empty()) {
            return false;
        }
        *lastPt = fPoints.back();
        return true;
    }

    void setLastPt(SkPoint lastPt) {
        if (fPoints.empty()) {
            moveTo(lastPt);
        } else {
            fPoints.back().set(lastPt.fX, lastPt.fY);
        }
    }

    const std::vector<uint8_t>& verbs() const { return fVerbs; }

    const std::vector<SkPoint>& points() const { return fPoints; }

private:
    std::vector<uint8_t> fVerbs;
    std::vector<SkPoint> fPoints;
};

/// this was more built into skia before, now it is in tools
class SkPathStroker2 {
public:
    // Returns the fill path
    SkPath getFillPath(const SkPath& path, const SkPaint& paint);

private:
    struct PathSegment {
        SkPath::Verb fVerb;
        SkPoint fPoints[4];
    };

    float fRadius;
    SkPaint::Cap fCap;
    SkPaint::Join fJoin;
    PathRecorder fInner, fOuter;

    // Initialize stroker state
    void initForPath(const SkPath& path, const SkPaint& paint);

    // Strokes a line segment
    void strokeLine(const PathSegment& line, bool needsMove);

    // Adds an endcap to fOuter
    enum class CapLocation { Start, End };
    void endcap(CapLocation loc);

    // Adds a join between the two segments
    void join(const PathSegment& prev, const PathSegment& curr);

    // Appends path in reverse to result
    static void appendPathReversed(const PathRecorder& path, PathRecorder* result);

    // Returns the segment unit normal
    static SkPoint unitNormal(const PathSegment& seg, float t);

    // Returns squared magnitude of line segments.
    static float squaredLineLength(const PathSegment& lineSeg);
};

void SkPathStroker2::initForPath(const SkPath& path, const SkPaint& paint) {
    fRadius = paint.getStrokeWidth() / 2;
    fCap = paint.getStrokeCap();
    fJoin = paint.getStrokeJoin();
    fInner.rewind();
    fOuter.rewind();
}

SkPath SkPathStroker2::getFillPath(const SkPath& path, const SkPaint& paint) {
    initForPath(path, paint);

    // Trace the inner and outer paths simultaneously. Inner will therefore be
    // recorded in reverse from how we trace the outline.
    SkPath::Iter it(path, false);
    PathSegment segment, prevSegment;
    bool firstSegment = true;
    while ((segment.fVerb = it.next(segment.fPoints)) != SkPath::kDone_Verb) {
        // Join to the previous segment
        if (!firstSegment) {
            join(prevSegment, segment);
        }

        // Stroke the current segment
        switch (segment.fVerb) {
            case SkPath::kLine_Verb:
                strokeLine(segment, firstSegment);
                break;
            case SkPath::kMove_Verb:
                // Don't care about multiple contours currently
                continue;
            default:
                SkDebugf("Unhandled path verb %d\n", segment.fVerb);
                break;
        }

        std::swap(segment, prevSegment);
        firstSegment = false;
    }

    // Open contour => endcap at the end
    const bool isClosed = path.isLastContourClosed();
    if (isClosed) {
        SkDebugf("Unhandled closed contour\n");
    } else {
        endcap(CapLocation::End);
    }

    // Walk inner path in reverse, appending to result
    appendPathReversed(fInner, &fOuter);
    endcap(CapLocation::Start);

    return fOuter.getPath();
}

void SkPathStroker2::strokeLine(const PathSegment& line, bool needsMove) {
    const SkPoint tangent = line.fPoints[1] - line.fPoints[0];
    const SkPoint normal = rotate90(tangent);
    const SkPoint offset = setLength(normal, fRadius);
    if (needsMove) {
        fOuter.moveTo(line.fPoints[0] + offset);
        fInner.moveTo(line.fPoints[0] - offset);
    }
    fOuter.lineTo(line.fPoints[1] + offset);
    fInner.lineTo(line.fPoints[1] - offset);
}

void SkPathStroker2::endcap(CapLocation loc) {
    const auto buttCap = [this](CapLocation loc) {
        if (loc == CapLocation::Start) {
            // Back at the start of the path: just close the stroked outline
            fOuter.close();
        } else {
            // Inner last pt == first pt when appending in reverse
            SkPoint innerLastPt;
            fInner.getLastPt(&innerLastPt);
            fOuter.lineTo(innerLastPt);
        }
    };

    switch (fCap) {
        case SkPaint::kButt_Cap:
            buttCap(loc);
            break;
        default:
            SkDebugf("Unhandled endcap %d\n", fCap);
            buttCap(loc);
            break;
    }
}

void SkPathStroker2::join(const PathSegment& prev, const PathSegment& curr) {
    const auto miterJoin = [this](const PathSegment& prev, const PathSegment& curr) {
        // Common path endpoint of the two segments is the midpoint of the miter line.
        const SkPoint miterMidpt = curr.fPoints[0];

        SkPoint before = unitNormal(prev, 1);
        SkPoint after = unitNormal(curr, 0);

        // Check who's inside and who's outside.
        PathRecorder *outer = &fOuter, *inner = &fInner;
        if (!isClockwise(before, after)) {
            std::swap(inner, outer);
            before = rotate180(before);
            after = rotate180(after);
        }

        const float cosTheta = before.dot(after);
        if (SkScalarNearlyZero(1 - cosTheta)) {
            // Nearly identical normals: don't bother.
            return;
        }

        // Before and after have the same origin and magnitude, so before+after is the diagonal of
        // their rhombus. Origin of this vector is the midpoint of the miter line.
        SkPoint miterVec = before + after;

        // Note the relationship (draw a right triangle with the miter line as its hypoteneuse):
        //     sin(theta/2) = strokeWidth / miterLength
        // so miterLength = strokeWidth / sin(theta/2)
        // where miterLength is the length of the miter from outer point to inner corner.
        // miterVec's origin is the midpoint of the miter line, so we use strokeWidth/2.
        // Sqrt is just an application of half-angle identities.
        const float sinHalfTheta = sqrtf(0.5 * (1 + cosTheta));
        const float halfMiterLength = fRadius / sinHalfTheta;
        miterVec.setLength(halfMiterLength);  // TODO: miter length limit

        // Outer: connect to the miter point, and then to t=0 (on outside stroke) of next segment.
        const SkPoint dest = setLength(after, fRadius);
        outer->lineTo(miterMidpt + miterVec);
        outer->lineTo(miterMidpt + dest);

        // Inner miter is more involved. We're already at t=1 (on inside stroke) of 'prev'.
        // Check 2 cases to see we can directly connect to the inner miter point
        // (midpoint - miterVec), or if we need to add extra "loop" geometry.
        const SkPoint prevUnitTangent = rotate90(before);
        const float radiusSquared = fRadius * fRadius;
        // 'alpha' is angle between prev tangent and the curr inwards normal
        const float cosAlpha = prevUnitTangent.dot(-after);
        // Solve triangle for len^2:  radius^2 = len^2 + (radius * sin(alpha))^2
        // This is the point at which the inside "corner" of curr at t=0 will lie on a
        // line connecting the inner and outer corners of prev at t=0. If len is below
        // this threshold, the inside corner of curr will "poke through" the start of prev,
        // and we'll need the inner loop geometry.
        const float threshold1 = radiusSquared * cosAlpha * cosAlpha;
        // Solve triangle for len^2:  halfMiterLen^2 = radius^2 + len^2
        // This is the point at which the inner miter point will lie on the inner stroke
        // boundary of the curr segment. If len is below this threshold, the miter point
        // moves 'inside' of the stroked outline, and we'll need the inner loop geometry.
        const float threshold2 = halfMiterLength * halfMiterLength - radiusSquared;
        // If a segment length is smaller than the larger of the two thresholds,
        // we'll have to add the inner loop geometry.
        const float maxLenSqd = std::max(threshold1, threshold2);
        const bool needsInnerLoop =
                squaredLineLength(prev) < maxLenSqd || squaredLineLength(curr) < maxLenSqd;
        if (needsInnerLoop) {
            // Connect to the miter midpoint (common path endpoint of the two segments),
            // and then to t=0 (on inside) of the next segment. This adds an interior "loop" of
            // geometry that handles edge cases where segment lengths are shorter than the
            // stroke width.
            inner->lineTo(miterMidpt);
            inner->lineTo(miterMidpt - dest);
        } else {
            // Directly connect to inner miter point.
            inner->setLastPt(miterMidpt - miterVec);
        }
    };

    switch (fJoin) {
        case SkPaint::kMiter_Join:
            miterJoin(prev, curr);
            break;
        default:
            SkDebugf("Unhandled join %d\n", fJoin);
            miterJoin(prev, curr);
            break;
    }
}

void SkPathStroker2::appendPathReversed(const PathRecorder& path, PathRecorder* result) {
    const int numVerbs = path.countVerbs();
    const int numPoints = path.countPoints();
    const std::vector<uint8_t>& verbs = path.verbs();
    const std::vector<SkPoint>& points = path.points();

    for (int i = numVerbs - 1, j = numPoints; i >= 0; i--) {
        auto verb = static_cast<SkPath::Verb>(verbs[i]);
        switch (verb) {
            case SkPath::kLine_Verb: {
                j -= 1;
                SkASSERT(j >= 1);
                result->lineTo(points[j - 1]);
                break;
            }
            case SkPath::kMove_Verb:
                // Ignore
                break;
            default:
                SkASSERT(false);
                break;
        }
    }
}

SkPoint SkPathStroker2::unitNormal(const PathSegment& seg, float t) {
    if (seg.fVerb != SkPath::kLine_Verb) {
        SkDebugf("Unhandled verb for unit normal %d\n", seg.fVerb);
    }

    (void)t;  // Not needed for lines
    const SkPoint tangent = seg.fPoints[1] - seg.fPoints[0];
    const SkPoint normal = rotate90(tangent);
    return setLength(normal, 1);
}

float SkPathStroker2::squaredLineLength(const PathSegment& lineSeg) {
    SkASSERT(lineSeg.fVerb == SkPath::kLine_Verb);
    const SkPoint diff = lineSeg.fPoints[1] - lineSeg.fPoints[0];
    return diff.dot(diff);
}

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

struct Skia {
    GrDirectContext *sk_context;

    Skia(GrDirectContext *sk_context) : sk_context(sk_context) { }
    
    static Skia *Context(VkEngine e) {
        static struct Skia *sk = null;
        if (sk) return sk;

        //GrBackendFormat gr_conv = GrBackendFormat::MakeVk(VK_FORMAT_R8G8B8_SRGB);
        Vulkan vk;
        GrVkBackendContext grc {
            vk->inst(),
            e->vk_gpu->phys,
            e->vk_device->device,
            e->vk_device->graphicsQueue,
            e->vk_gpu->indices.graphicsFamily.value(),
            vk->version
        };
        //grc.fVkExtensions -- not sure if we need to populate this with our extensions, but it has no interface to do so
        grc.fMaxAPIVersion = vk->version;


        //grc.fVkExtensions = new GrVkExtensions(); // internal needs population perhaps
        grc.fGetProc = [](cchar_t *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
            return (dev == VK_NULL_HANDLE) ? vkGetInstanceProcAddr(inst, name) :
                                             vkGetDeviceProcAddr  (dev,  name);
        };

        static sk_sp<GrDirectContext> ctx = GrDirectContext::MakeVulkan(grc);
 
        assert(ctx);
        sk = new Skia(ctx.get());
        return sk;
    }
};

/// canvas renders to image, and can manage the renderer/resizing
struct ICanvas {
    GrDirectContext     *ctx = null;
    VkEngine               e = null;
    VkhPresenter    renderer = null;
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    VkhImage        vk_image = null;
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

    /// can be given an image
    void canvas_resize(VkhImage &image, int width, int height) {
        if (vk_image)
            vkh_image_drop(vk_image);
        if (!image) {
            vk_image = vkh_image_create(
                e->vkh, VK_FORMAT_R8G8B8A8_UNORM, u32(width), u32(height),
                VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

            // test code to clear the image with a color (no result from SkCanvas currently!)
            // this results in a color drawn after the renderer blits the VkImage to the window
            // this image is given to 
            VkDevice device = e->vk_device->device;
            VkQueue  queue  = e->renderer->queue;
            VkImage  image  = vk_image->image;

            VkCommandBuffer commandBuffer = e->vk_device->command_begin();

            // Assume you have a VkDevice, VkPhysicalDevice, VkQueue, and VkCommandBuffer already set up.
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            // Clear the image with blue color
            VkClearColorValue clearColor = { 0.4f, 0.0f, 0.5f, 1.0f };
            vkCmdClearColorImage(
                commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearColor,
                1,
                &barrier.subresourceRange
            );

            // Transition image layout to SHADER_READ_ONLY_OPTIMAL
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            e->vk_device->command_submit(commandBuffer);

        } else {
            /// option: we can know dpi here as declared by the user
            vk_image = vkh_image_grab(image);
            assert(width == vk_image->width && height == vk_image->height);
        }
        
        sz = vec2i { width, height };
        ///
        ctx                     = Skia::Context(e)->sk_context;
        auto imi                = GrVkImageInfo { };
        imi.fImage              = vk_image->image;
        imi.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
        imi.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imi.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
        imi.fImageUsageFlags    = VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // i dont think so.
        imi.fSampleCount        = 1;
        imi.fLevelCount         = 1;
        imi.fCurrentQueueFamily = e->vk_gpu->indices.graphicsFamily.value();
        imi.fProtected          = GrProtected::kNo;
        imi.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        auto color_space = SkColorSpace::MakeSRGB();
        auto rt = GrBackendRenderTarget { sz.x, sz.y, imi };
        sk_surf = SkSurfaces::WrapBackendRenderTarget(ctx, rt,
                    kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
                    color_space, null);
        sk_canvas = sk_surf->getCanvas();
        dpi_scale = e->vk_gpu->dpi_scale;
        identity();
    }

    void app_resize() {
        /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
        float sx, sy;
        u32 width, height;
        vkh_presenter_get_size(renderer, &width, &height, &sx, &sy); /// vkh/vk should have both vk engine and glfw facility
        VkhImage img = null;
        canvas_resize(img, width, height);
        vkh_presenter_build_blit_cmd(renderer, vk_image->image,
            width / dpi_scale.x, height / dpi_scale.y);
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


    void    clear()        { sk_canvas->clear(sk_color(top->color)); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        ctx->flush();
        ctx->submit();
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
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function
        pos.x = mix(rect.x, rect.x + rect.w - isz.x * sc, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - isz.y * sc, align.y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler cubicResampler { 1.0f/3, 1.0f/3 };
        SkSamplingOptions samplingOptions(cubicResampler);

        sk_canvas->scale(sc, sc);
        SkImage *img = im->get();
        sk_canvas->drawImage(img, SkScalar(0), SkScalar(0), samplingOptions);
        sk_canvas->restore();

        if (!attach_tx) {
            delete im;
            ctx->flush();
            ctx->submit();
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

    void clip(rectd &path) {
        assert(false);
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
        graphics::shape sh = rect;
        SkRect r = { float(rect.x), float(rect.y), float(rect.x + rect.w), float(rect.y + rect.h) };
        sk_canvas->drawRect(r, ps);
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
            auto t = SkTypeface::MakeFromFile((symbol)p.cs());
            font->sk_font = new SkFont(t);
            ((SkFont*)font->sk_font)->setSize(font->sz);
        }
        return *(SkFont*)font->sk_font;
    }
    type_register(ICanvas);
};

mx_implement(Canvas, mx);

// we need to give VkEngine too, if we want to support image null
Canvas::Canvas(VkhImage image) : Canvas() {
    data->e = image->vkh->e;
    data->canvas_resize(image, image->width, image->height);
    data->save();
}

Canvas::Canvas(VkhPresenter renderer) : Canvas() {
    data->e = renderer->vkh->e;
    data->renderer = renderer;
    data->app_resize();
    data->save();
}

u32 Canvas::get_virtual_width()  { return data->sz.x / data->dpi_scale.x; }
u32 Canvas::get_virtual_height() { return data->sz.y / data->dpi_scale.y; }

void Canvas::canvas_resize(VkhImage image, int width, int height) {
    return data->canvas_resize(image, width, height);
}

void Canvas::app_resize() {
    return data->app_resize();
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

void Canvas::arc(glm::vec3 pos, real radius, real startAngle, real endAngle, bool is_fill) {
    return data->arc(pos, radius, startAngle, endAngle, is_fill);
}

text_metrics Canvas::measure(str text) {
    return data->measure(text);
}

double Canvas::measure_advance(char *text, size_t len) {
    return data->measure_advance(text, len);
}

str     Canvas::ellipsis(str text, rectd rect, text_metrics &tm) {
    return data->ellipsis(text, rect, tm);
}

void    Canvas::image(ion::image img, rectd rect, alignment align, vec2d offset, bool attach_tx) {
    return data->image(img, rect, align, offset, attach_tx);
}

void    Canvas::text(str text, rectd rect, alignment align, vec2d offset, bool ellip, rectd *placement) {
    return data->text(text, rect, align, offset, ellip, placement);
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

void    Canvas::outline(array<glm::vec3> v3) {
    return data->outline(v3);
}

void Canvas::line(glm::vec3 &a, glm::vec3 &b) {
    return data->line(a, b);
}

void    Canvas::outline(array<glm::vec2> line) {
    return data->outline(line);
}

void  Canvas::projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
    return data->projection(m, v, p);
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

struct iSVG {
    sk_sp<SkSVGDOM> svg_dom;
    int             w, h;
};

mx_implement(SVG, mx);

SVG::SVG(path p) : SVG() {
    SkStream* stream = new SkFILEStream((symbol)p.cs());
    data->svg_dom = SkSVGDOM::MakeFromStream(*stream);
    delete stream;
}

void SVG::render(Canvas &canvas, int w, int h) {
    if (data->w != w || data->h != h) {
        data->w  = w;
        data->h  = h;
        //data->svg_dom->setContainerSize(SkSize::Make(w, h));
    }
    //data->svg_dom->render(canvas.data->sk_canvas);
}