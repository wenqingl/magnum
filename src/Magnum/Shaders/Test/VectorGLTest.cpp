/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Path.h>

#ifdef CORRADE_TARGET_APPLE
#include <Corrade/Containers/Pair.h>
#include <Corrade/Utility/System.h> /* isSandboxed() */
#endif

#include "Magnum/Image.h"
#include "Magnum/ImageView.h"
#include "Magnum/PixelFormat.h"
#include "Magnum/DebugTools/CompareImage.h"
#include "Magnum/GL/Framebuffer.h"
#include "Magnum/GL/Mesh.h"
#include "Magnum/GL/OpenGLTester.h"
#include "Magnum/GL/Renderbuffer.h"
#include "Magnum/GL/RenderbufferFormat.h"
#include "Magnum/GL/Texture.h"
#include "Magnum/GL/TextureFormat.h"
#include "Magnum/Math/Color.h"
#include "Magnum/Math/Matrix3.h"
#include "Magnum/Math/Matrix4.h"
#include "Magnum/MeshTools/Compile.h"
#include "Magnum/Primitives/Plane.h"
#include "Magnum/Primitives/Square.h"
#include "Magnum/Shaders/VectorGL.h"
#include "Magnum/Trade/AbstractImporter.h"
#include "Magnum/Trade/ImageData.h"
#include "Magnum/Trade/MeshData.h"

#ifndef MAGNUM_TARGET_GLES2
#include "Magnum/GL/Extensions.h"
#include "Magnum/GL/MeshView.h"
#include "Magnum/MeshTools/Concatenate.h"
#include "Magnum/MeshTools/GenerateIndices.h"
#include "Magnum/Primitives/Circle.h"
#include "Magnum/Primitives/Cone.h"
#include "Magnum/Primitives/Plane.h"
#include "Magnum/Primitives/Square.h"
#include "Magnum/Primitives/UVSphere.h"
#include "Magnum/Shaders/Generic.h"
#include "Magnum/Shaders/Generic.h"
#include "Magnum/Shaders/Vector.h"
#endif

#include "configure.h"

namespace Magnum { namespace Shaders { namespace Test { namespace {

struct VectorGLTest: GL::OpenGLTester {
    explicit VectorGLTest();

    template<UnsignedInt dimensions> void construct();
    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void constructUniformBuffers();
    #endif

    template<UnsignedInt dimensions> void constructMove();
    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void constructMoveUniformBuffers();
    #endif

    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void constructUniformBuffersInvalid();
    #endif

    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void setUniformUniformBuffersEnabled();
    template<UnsignedInt dimensions> void bindBufferUniformBuffersNotEnabled();
    #endif
    template<UnsignedInt dimensions> void setTextureMatrixNotEnabled();
    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void bindTextureTransformBufferNotEnabled();
    #endif
    #ifndef MAGNUM_TARGET_GLES2
    template<UnsignedInt dimensions> void setWrongDrawOffset();
    #endif

    void renderSetup();
    void renderTeardown();

    template<VectorGL2D::Flag flag = VectorGL2D::Flag{}> void renderDefaults2D();
    template<VectorGL3D::Flag flag = VectorGL3D::Flag{}> void renderDefaults3D();
    template<VectorGL2D::Flag flag = VectorGL2D::Flag{}> void render2D();
    template<VectorGL3D::Flag flag = VectorGL3D::Flag{}> void render3D();

    #ifndef MAGNUM_TARGET_GLES2
    void renderMulti2D();
    void renderMulti3D();
    #endif

    private:
        PluginManager::Manager<Trade::AbstractImporter> _manager{"nonexistent"};
        Containers::String _testDir;

        GL::Renderbuffer _color{NoCreate};
        #ifndef MAGNUM_TARGET_GLES2
        GL::Renderbuffer _objectId{NoCreate};
        #endif
        GL::Framebuffer _framebuffer{NoCreate};
};

/*
    Rendering tests done:

    [B] base
    [O] UBOs + draw offset
    [M] multidraw

    Mesa Intel                      BOM
               ES2                   xx
               ES3                  BOx
    Mesa AMD                        B
    Mesa llvmpipe                   B
    SwiftShader ES2                 Bxx
                ES3                 B
    ANGLE ES2                        xx
          ES3                       BOM
    ARM Mali (Huawei P10) ES2       Bxx
                          ES3       BOx
    WebGL (on Mesa Intel) 1.0       Bxx
                          2.0       BOM
    NVidia
    Intel Windows
    AMD macOS                         x
    Intel macOS                     BOx
    iPhone 6 w/ iOS 12.4 ES3        B x
*/

using namespace Math::Literals;

constexpr struct {
    const char* name;
    VectorGL2D::Flags flags;
} ConstructData[]{
    {"", {}},
    {"texture transformation", VectorGL2D::Flag::TextureTransformation}
};

#ifndef MAGNUM_TARGET_GLES2
constexpr struct {
    const char* name;
    VectorGL2D::Flags flags;
    UnsignedInt materialCount, drawCount;
} ConstructUniformBuffersData[]{
    {"classic fallback", {}, 1, 1},
    {"", VectorGL2D::Flag::UniformBuffers, 1, 1},
    {"texture transformation", VectorGL2D::Flag::UniformBuffers|VectorGL2D::Flag::TextureTransformation, 1, 1},
    /* SwiftShader has 256 uniform vectors at most, per-draw is 4+1 in 3D case
       and 3+1 in 2D, per-material 3 */
    {"multiple materials, draws", VectorGL2D::Flag::UniformBuffers, 15, 42},
    {"multidraw with all the things", VectorGL2D::Flag::MultiDraw|VectorGL2D::Flag::TextureTransformation, 15, 42}
};
#endif

#ifndef MAGNUM_TARGET_GLES2
constexpr struct {
    const char* name;
    VectorGL2D::Flags flags;
    UnsignedInt materialCount, drawCount;
    const char* message;
} ConstructUniformBuffersInvalidData[]{
    {"zero draws", VectorGL2D::Flag::UniformBuffers, 1, 0,
        "draw count can't be zero"},
    {"zero materials", VectorGL2D::Flag::UniformBuffers, 0, 1,
        "material count can't be zero"},
};
#endif

const struct {
    const char* name;
    VectorGL2D::Flags flags;
    Matrix3 textureTransformation;
    Color4 backgroundColor, color;
    const char* file2D;
    const char* file3D;
    bool flip;
} RenderData[] {
    {"texture transformation", VectorGL2D::Flag::TextureTransformation,
        Matrix3::translation(Vector2{1.0f})*Matrix3::scaling(Vector2{-1.0f}),
        0x00000000_rgbaf, 0xffffff_rgbf,
        "defaults.tga", "defaults.tga", true},
    {"", {}, {}, 0x9999ff_rgbf, 0xffff99_rgbf,
        "vector2D.tga", "vector3D.tga", false}
};

#ifndef MAGNUM_TARGET_GLES2
constexpr struct {
    const char* name;
    const char* expected2D;
    const char* expected3D;
    VectorGL2D::Flags flags;
    UnsignedInt materialCount, drawCount;
    UnsignedInt uniformIncrement;
    Float maxThreshold, meanThreshold;
} RenderMultiData[] {
    {"bind with offset", "multidraw2D.tga", "multidraw3D.tga",
        {}, 1, 1, 16,
        /* Minor differences on ARM Mali */
        1.34f, 0.02f},
    {"draw offset", "multidraw2D.tga", "multidraw3D.tga",
        {}, 2, 3, 1,
        /* Minor differences on ARM Mali */
        1.34f, 0.02f},
    {"multidraw", "multidraw2D.tga", "multidraw3D.tga",
        VectorGL2D::Flag::MultiDraw, 2, 3, 1,
        /* Minor differences on ARM Mali */
        1.34f, 0.02f},
};
#endif

VectorGLTest::VectorGLTest() {
    addInstancedTests<VectorGLTest>({
        &VectorGLTest::construct<2>,
        &VectorGLTest::construct<3>},
        Containers::arraySize(ConstructData));

    #ifndef MAGNUM_TARGET_GLES2
    addInstancedTests<VectorGLTest>({
        &VectorGLTest::constructUniformBuffers<2>,
        &VectorGLTest::constructUniformBuffers<3>},
        Containers::arraySize(ConstructUniformBuffersData));
    #endif

    addTests<VectorGLTest>({
        &VectorGLTest::constructMove<2>,
        &VectorGLTest::constructMove<3>,

        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::constructMoveUniformBuffers<2>,
        &VectorGLTest::constructMoveUniformBuffers<3>,
        #endif
        });

    #ifndef MAGNUM_TARGET_GLES2
    addInstancedTests<VectorGLTest>({
        &VectorGLTest::constructUniformBuffersInvalid<2>,
        &VectorGLTest::constructUniformBuffersInvalid<3>},
        Containers::arraySize(ConstructUniformBuffersInvalidData));
    #endif

    addTests<VectorGLTest>({
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::setUniformUniformBuffersEnabled<2>,
        &VectorGLTest::setUniformUniformBuffersEnabled<3>,
        &VectorGLTest::bindBufferUniformBuffersNotEnabled<2>,
        &VectorGLTest::bindBufferUniformBuffersNotEnabled<3>,
        #endif
        &VectorGLTest::setTextureMatrixNotEnabled<2>,
        &VectorGLTest::setTextureMatrixNotEnabled<3>,
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::bindTextureTransformBufferNotEnabled<2>,
        &VectorGLTest::bindTextureTransformBufferNotEnabled<3>,
        #endif
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::setWrongDrawOffset<2>,
        &VectorGLTest::setWrongDrawOffset<3>
        #endif
        });

    /* MSVC needs explicit type due to default template args */
    addTests<VectorGLTest>({
        &VectorGLTest::renderDefaults2D,
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::renderDefaults2D<VectorGL2D::Flag::UniformBuffers>,
        #endif
        &VectorGLTest::renderDefaults3D,
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::renderDefaults3D<VectorGL3D::Flag::UniformBuffers>,
        #endif
        },
        &VectorGLTest::renderSetup,
        &VectorGLTest::renderTeardown);

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<VectorGLTest>({
        &VectorGLTest::render2D,
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::render2D<VectorGL2D::Flag::UniformBuffers>,
        #endif
        &VectorGLTest::render3D,
        #ifndef MAGNUM_TARGET_GLES2
        &VectorGLTest::render3D<VectorGL3D::Flag::UniformBuffers>,
        #endif
        },
        Containers::arraySize(RenderData),
        &VectorGLTest::renderSetup,
        &VectorGLTest::renderTeardown);

    #ifndef MAGNUM_TARGET_GLES2
    addInstancedTests({&VectorGLTest::renderMulti2D,
                       &VectorGLTest::renderMulti3D},
        Containers::arraySize(RenderMultiData),
        &VectorGLTest::renderSetup,
        &VectorGLTest::renderTeardown);
    #endif

    /* Load the plugins directly from the build tree. Otherwise they're either
       static and already loaded or not present in the build tree */
    #ifdef ANYIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ANYIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef TGAIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(TGAIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    #ifdef CORRADE_TARGET_APPLE
    if(Utility::System::isSandboxed()
        #if defined(CORRADE_TARGET_IOS) && defined(CORRADE_TESTSUITE_TARGET_XCTEST)
        /** @todo Fix this once I persuade CMake to run XCTest tests properly */
        && std::getenv("SIMULATOR_UDID")
        #endif
    ) {
        _testDir = Utility::Path::split(*Utility::Path::executableLocation()).first();
    } else
    #endif
    {
        _testDir = SHADERS_TEST_DIR;
    }
}

template<UnsignedInt dimensions> void VectorGLTest::construct() {
    setTestCaseTemplateName(std::to_string(dimensions));

    auto&& data = ConstructData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    VectorGL<dimensions> shader{data.flags};
    CORRADE_COMPARE(shader.flags(), data.flags);
    CORRADE_VERIFY(shader.id());
    {
        #if defined(CORRADE_TARGET_APPLE) && !defined(MAGNUM_TARGET_GLES)
        CORRADE_EXPECT_FAIL("macOS drivers need insane amount of state to validate properly.");
        #endif
        CORRADE_VERIFY(shader.validate().first);
    }

    MAGNUM_VERIFY_NO_GL_ERROR();
}

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::constructUniformBuffers() {
    setTestCaseTemplateName(std::to_string(dimensions));

    auto&& data = ConstructUniformBuffersData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifndef MAGNUM_TARGET_GLES
    if((data.flags & VectorGL<dimensions>::Flag::UniformBuffers) && !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    if(data.flags >= VectorGL2D::Flag::MultiDraw) {
        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::shader_draw_parameters>())
            CORRADE_SKIP(GL::Extensions::ARB::shader_draw_parameters::string() << "is not supported.");
        #elif !defined(MAGNUM_TARGET_WEBGL)
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ANGLE::multi_draw>())
            CORRADE_SKIP(GL::Extensions::ANGLE::multi_draw::string() << "is not supported.");
        #else
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::WEBGL::multi_draw>())
            CORRADE_SKIP(GL::Extensions::WEBGL::multi_draw::string() << "is not supported.");
        #endif
    }

    VectorGL<dimensions> shader{data.flags, data.materialCount, data.drawCount};
    CORRADE_COMPARE(shader.flags(), data.flags);
    CORRADE_COMPARE(shader.drawCount(), data.drawCount);
    CORRADE_VERIFY(shader.id());
    {
        #if defined(CORRADE_TARGET_APPLE) && !defined(MAGNUM_TARGET_GLES)
        CORRADE_EXPECT_FAIL("macOS drivers need insane amount of state to validate properly.");
        #endif
        CORRADE_VERIFY(shader.validate().first);
    }

    MAGNUM_VERIFY_NO_GL_ERROR();
}
#endif

template<UnsignedInt dimensions> void VectorGLTest::constructMove() {
    setTestCaseTemplateName(std::to_string(dimensions));

    VectorGL<dimensions> a{VectorGL<dimensions>::Flag::TextureTransformation};
    const GLuint id = a.id();
    CORRADE_VERIFY(id);

    MAGNUM_VERIFY_NO_GL_ERROR();

    VectorGL<dimensions> b{std::move(a)};
    CORRADE_COMPARE(b.id(), id);
    CORRADE_COMPARE(b.flags(), VectorGL<dimensions>::Flag::TextureTransformation);
    CORRADE_VERIFY(!a.id());

    VectorGL<dimensions> c{NoCreate};
    c = std::move(b);
    CORRADE_COMPARE(c.id(), id);
    CORRADE_COMPARE(c.flags(), VectorGL<dimensions>::Flag::TextureTransformation);
    CORRADE_VERIFY(!b.id());
}

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::constructMoveUniformBuffers() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    VectorGL<dimensions> a{VectorGL<dimensions>::Flag::UniformBuffers, 2, 5};
    const GLuint id = a.id();
    CORRADE_VERIFY(id);

    MAGNUM_VERIFY_NO_GL_ERROR();

    VectorGL<dimensions> b{std::move(a)};
    CORRADE_COMPARE(b.id(), id);
    CORRADE_COMPARE(b.flags(), VectorGL<dimensions>::Flag::UniformBuffers);
    CORRADE_COMPARE(b.materialCount(), 2);
    CORRADE_COMPARE(b.drawCount(), 5);
    CORRADE_VERIFY(!a.id());

    VectorGL<dimensions> c{NoCreate};
    c = std::move(b);
    CORRADE_COMPARE(c.id(), id);
    CORRADE_COMPARE(c.flags(), VectorGL<dimensions>::Flag::UniformBuffers);
    CORRADE_COMPARE(c.materialCount(), 2);
    CORRADE_COMPARE(c.drawCount(), 5);
    CORRADE_VERIFY(!b.id());
}
#endif

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::constructUniformBuffersInvalid() {
    auto&& data = ConstructUniformBuffersInvalidData[testCaseInstanceId()];
    setTestCaseTemplateName(std::to_string(dimensions));
    setTestCaseDescription(data.name);

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    std::ostringstream out;
    Error redirectError{&out};
    VectorGL<dimensions>{data.flags, data.materialCount, data.drawCount};
    CORRADE_COMPARE(out.str(), Utility::formatString(
        "Shaders::VectorGL: {}\n", data.message));
}
#endif

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::setUniformUniformBuffersEnabled() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    std::ostringstream out;
    Error redirectError{&out};

    VectorGL<dimensions> shader{VectorGL<dimensions>::Flag::UniformBuffers};
    shader.setTransformationProjectionMatrix({})
        .setTextureMatrix({})
        .setBackgroundColor({})
        .setColor({});
    CORRADE_COMPARE(out.str(),
        "Shaders::VectorGL::setTransformationProjectionMatrix(): the shader was created with uniform buffers enabled\n"
        "Shaders::VectorGL::setTextureMatrix(): the shader was created with uniform buffers enabled\n"
        "Shaders::VectorGL::setBackgroundColor(): the shader was created with uniform buffers enabled\n"
        "Shaders::VectorGL::setColor(): the shader was created with uniform buffers enabled\n");
}

template<UnsignedInt dimensions> void VectorGLTest::bindBufferUniformBuffersNotEnabled() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    std::ostringstream out;
    Error redirectError{&out};

    GL::Buffer buffer;
    VectorGL<dimensions> shader;
    shader.bindTransformationProjectionBuffer(buffer)
          .bindTransformationProjectionBuffer(buffer, 0, 16)
          .bindDrawBuffer(buffer)
          .bindDrawBuffer(buffer, 0, 16)
          .bindTextureTransformationBuffer(buffer)
          .bindTextureTransformationBuffer(buffer, 0, 16)
          .bindMaterialBuffer(buffer)
          .bindMaterialBuffer(buffer, 0, 16)
          .setDrawOffset(0);
    CORRADE_COMPARE(out.str(),
        "Shaders::VectorGL::bindTransformationProjectionBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindTransformationProjectionBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindDrawBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindDrawBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindTextureTransformationBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindTextureTransformationBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindMaterialBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::bindMaterialBuffer(): the shader was not created with uniform buffers enabled\n"
        "Shaders::VectorGL::setDrawOffset(): the shader was not created with uniform buffers enabled\n");
}
#endif

template<UnsignedInt dimensions> void VectorGLTest::setTextureMatrixNotEnabled() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    std::ostringstream out;
    Error redirectError{&out};

    VectorGL<dimensions> shader;
    shader.setTextureMatrix({});

    CORRADE_COMPARE(out.str(),
        "Shaders::VectorGL::setTextureMatrix(): the shader was not created with texture transformation enabled\n");
}

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::bindTextureTransformBufferNotEnabled() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    std::ostringstream out;
    Error redirectError{&out};

    GL::Buffer buffer{GL::Buffer::TargetHint::Uniform};
    VectorGL<dimensions> shader{VectorGL<dimensions>::Flag::UniformBuffers};
    shader.bindTextureTransformationBuffer(buffer)
          .bindTextureTransformationBuffer(buffer, 0, 16);
    CORRADE_COMPARE(out.str(),
        "Shaders::VectorGL::bindTextureTransformationBuffer(): the shader was not created with texture transformation enabled\n"
        "Shaders::VectorGL::bindTextureTransformationBuffer(): the shader was not created with texture transformation enabled\n");
}
#endif

#ifndef MAGNUM_TARGET_GLES2
template<UnsignedInt dimensions> void VectorGLTest::setWrongDrawOffset() {
    setTestCaseTemplateName(std::to_string(dimensions));

    #ifdef CORRADE_NO_ASSERT
    CORRADE_SKIP("CORRADE_NO_ASSERT defined, can't test assertions");
    #endif

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    std::ostringstream out;
    Error redirectError{&out};
    VectorGL<dimensions>{VectorGL<dimensions>::Flag::UniformBuffers, 2, 5}
        .setDrawOffset(5);
    CORRADE_COMPARE(out.str(),
        "Shaders::VectorGL::setDrawOffset(): draw offset 5 is out of bounds for 5 draws\n");
}
#endif

constexpr Vector2i RenderSize{80, 80};

void VectorGLTest::renderSetup() {
    /* Pick a color that's directly representable on RGBA4 as well to reduce
       artifacts */
    GL::Renderer::setClearColor(0x111111_rgbf);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    _color = GL::Renderbuffer{};
    _color.setStorage(
        #if !defined(MAGNUM_TARGET_GLES2) || !defined(MAGNUM_TARGET_WEBGL)
        GL::RenderbufferFormat::RGBA8,
        #else
        GL::RenderbufferFormat::RGBA4,
        #endif
        RenderSize);
    _framebuffer = GL::Framebuffer{{{}, RenderSize}};
    _framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, _color)
        .clear(GL::FramebufferClear::Color)
        .bind();
}

void VectorGLTest::renderTeardown() {
    _framebuffer = GL::Framebuffer{NoCreate};
    _color = GL::Renderbuffer{NoCreate};
}

constexpr GL::TextureFormat TextureFormatR =
    #ifndef MAGNUM_TARGET_GLES2
    GL::TextureFormat::R8
    #else
    GL::TextureFormat::Luminance
    #endif
    ;

template<VectorGL2D::Flag flag> void VectorGLTest::renderDefaults2D() {
    #ifndef MAGNUM_TARGET_GLES2
    if(flag == VectorGL2D::Flag::UniformBuffers) {
        setTestCaseTemplateName("Flag::UniformBuffers");

        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
            CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
        #endif
    }
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    GL::Mesh square = MeshTools::compile(Primitives::squareSolid(Primitives::SquareFlag::TextureCoordinates));

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    GL::Texture2D texture;
    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    texture.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge);

    #ifdef MAGNUM_TARGET_GLES2
    /* Don't want to bother with the fiasco of single-channel formats and
       texture storage extensions on ES2 */
    texture.setImage(0, TextureFormatR, *image);
    #else
    texture.setStorage(1, TextureFormatR, image->size())
        .setSubImage(0, {}, *image);
    #endif

    VectorGL2D shader{flag};
    shader.bindVectorTexture(texture);

    if(flag == VectorGL2D::Flag{}) {
        shader.draw(square);
    }
    #ifndef MAGNUM_TARGET_GLES2
    else if(flag == VectorGL2D::Flag::UniformBuffers) {
        GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, {
            TransformationProjectionUniform2D{}
        }};
        GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, {
            VectorDrawUniform{}
        }};
        GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, {
            VectorMaterialUniform{}
        }};
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindMaterialBuffer(materialUniform)
            .draw(square);
    }
    #endif
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    MAGNUM_VERIFY_NO_GL_ERROR();

    #if !(defined(MAGNUM_TARGET_GLES2) && defined(MAGNUM_TARGET_WEBGL))
    /* SwiftShader has off-by-one differences on edges, ARM Mali a bit more of
       them, llvmpipe is off-by-two */
    const Float maxThreshold = 2.0f, meanThreshold = 0.071f;
    #else
    /* WebGL 1 doesn't have 8bit renderbuffer storage, so it's way worse */
    const Float maxThreshold = 17.0f, meanThreshold = 0.359f;
    #endif
    CORRADE_COMPARE_WITH(
        /* Dropping the alpha channel, as it's always 1.0 */
        Containers::arrayCast<Color3ub>(_framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm}).pixels<Color4ub>()),
        Utility::Path::join(_testDir, "VectorTestFiles/defaults.tga"),
        (DebugTools::CompareImageToFile{_manager, maxThreshold, meanThreshold}));
}

template<VectorGL3D::Flag flag> void VectorGLTest::renderDefaults3D() {
    #ifndef MAGNUM_TARGET_GLES2
    if(flag == VectorGL3D::Flag::UniformBuffers) {
        setTestCaseTemplateName("Flag::UniformBuffers");

        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
            CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
        #endif
    }
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    GL::Mesh plane = MeshTools::compile(Primitives::planeSolid(Primitives::PlaneFlag::TextureCoordinates));

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    GL::Texture2D texture;
    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    texture.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge);

    #ifdef MAGNUM_TARGET_GLES2
    /* Don't want to bother with the fiasco of single-channel formats and
       texture storage extensions on ES2 */
    texture.setImage(0, TextureFormatR, *image);
    #else
    texture.setStorage(1, TextureFormatR, image->size())
        .setSubImage(0, {}, *image);
    #endif

    VectorGL3D shader{flag};
    shader.bindVectorTexture(texture);

    if(flag == VectorGL3D::Flag{}) {
        shader.draw(plane);
    }
    #ifndef MAGNUM_TARGET_GLES2
    else if(flag == VectorGL2D::Flag::UniformBuffers) {
        GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, {
            TransformationProjectionUniform3D{}
        }};
        GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, {
            VectorDrawUniform{}
        }};
        GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, {
            VectorMaterialUniform{}
        }};
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindMaterialBuffer(materialUniform)
            .draw(plane);
    }
    #endif
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    MAGNUM_VERIFY_NO_GL_ERROR();

    #if !(defined(MAGNUM_TARGET_GLES2) && defined(MAGNUM_TARGET_WEBGL))
    /* SwiftShader has off-by-one differences on edges, ARM Mali a bit more of
       them; llvmpipe is off-by-two */
    const Float maxThreshold = 2.0f, meanThreshold = 0.071f;
    #else
    /* WebGL 1 doesn't have 8bit renderbuffer storage, so it's way worse */
    const Float maxThreshold = 17.0f, meanThreshold = 0.359f;
    #endif
    CORRADE_COMPARE_WITH(
        /* Dropping the alpha channel, as it's always 1.0 */
        Containers::arrayCast<Color3ub>(_framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm}).pixels<Color4ub>()),
        Utility::Path::join(_testDir, "VectorTestFiles/defaults.tga"),
        (DebugTools::CompareImageToFile{_manager, maxThreshold, meanThreshold}));
}

template<VectorGL2D::Flag flag> void VectorGLTest::render2D() {
    auto&& data = RenderData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifndef MAGNUM_TARGET_GLES2
    if(flag == VectorGL2D::Flag::UniformBuffers) {
        setTestCaseTemplateName("Flag::UniformBuffers");

        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
            CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
        #endif
    }
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    GL::Mesh square = MeshTools::compile(Primitives::squareSolid(Primitives::SquareFlag::TextureCoordinates));

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    GL::Texture2D texture;
    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    texture.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge);

    #ifdef MAGNUM_TARGET_GLES2
    /* Don't want to bother with the fiasco of single-channel formats and
       texture storage extensions on ES2 */
    texture.setImage(0, TextureFormatR, *image);
    #else
    texture.setStorage(1, TextureFormatR, image->size())
        .setSubImage(0, {}, *image);
    #endif

    VectorGL2D shader{data.flags|flag};
    shader.bindVectorTexture(texture);

    if(flag == VectorGL2D::Flag{}) {
        shader.setBackgroundColor(data.backgroundColor)
            .setColor(data.color);
        if(data.textureTransformation != Matrix3{})
            shader.setTextureMatrix(data.textureTransformation);
        else shader.setTransformationProjectionMatrix(
            Matrix3::projection({2.1f, 2.1f})*
            Matrix3::rotation(5.0_degf));
        shader.draw(square);
    }
    #ifndef MAGNUM_TARGET_GLES2
    else if(flag == VectorGL2D::Flag::UniformBuffers) {
        GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, {
            TransformationProjectionUniform2D{}
                .setTransformationProjectionMatrix(
                    data.textureTransformation == Matrix3{} ?
                        Matrix3::projection({2.1f, 2.1f})*
                        Matrix3::rotation(5.0_degf) : Matrix3{}
                )
        }};
        GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, {
            VectorDrawUniform{}
        }};
        GL::Buffer textureTransformationUniform{GL::Buffer::TargetHint::Uniform, {
            TextureTransformationUniform{}
                .setTextureMatrix(data.textureTransformation)
        }};
        GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, {
            VectorMaterialUniform{}
                .setBackgroundColor(data.backgroundColor)
                .setColor(data.color)
        }};
        if(data.flags & VectorGL2D::Flag::TextureTransformation)
            shader.bindTextureTransformationBuffer(textureTransformationUniform);
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindMaterialBuffer(materialUniform)
            .draw(square);
    }
    #endif
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    MAGNUM_VERIFY_NO_GL_ERROR();

    Image2D rendered = _framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm});
    /* Dropping the alpha channel, as it's always 1.0 */
    Containers::StridedArrayView2D<Color3ub> pixels =
        Containers::arrayCast<Color3ub>(rendered.pixels<Color4ub>());
    if(data.flip) pixels = pixels.flipped<0>().flipped<1>();

    #if !(defined(MAGNUM_TARGET_GLES2) && defined(MAGNUM_TARGET_WEBGL))
    /* SwiftShader has differently rasterized edges on four pixels */
    const Float maxThreshold = 170.0f, meanThreshold = 0.146f;
    #else
    /* WebGL 1 doesn't have 8bit renderbuffer storage, so it's way worse */
    const Float maxThreshold = 170.0f, meanThreshold = 0.962f;
    #endif
    CORRADE_COMPARE_WITH(pixels,
        Utility::Path::join({_testDir, "VectorTestFiles", data.file2D}),
        (DebugTools::CompareImageToFile{_manager, maxThreshold, meanThreshold}));
}

template<VectorGL3D::Flag flag> void VectorGLTest::render3D() {
    auto&& data = RenderData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifndef MAGNUM_TARGET_GLES2
    if(flag == VectorGL3D::Flag::UniformBuffers) {
        setTestCaseTemplateName("Flag::UniformBuffers");

        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
            CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
        #endif
    }
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    GL::Mesh plane = MeshTools::compile(Primitives::planeSolid(Primitives::PlaneFlag::TextureCoordinates));

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    GL::Texture2D texture;
    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    texture.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge);

    #ifdef MAGNUM_TARGET_GLES2
    /* Don't want to bother with the fiasco of single-channel formats and
       texture storage extensions on ES2 */
    texture.setImage(0, TextureFormatR, *image);
    #else
    texture.setStorage(1, TextureFormatR, image->size())
        .setSubImage(0, {}, *image);
    #endif

    VectorGL3D shader{data.flags|flag};
    shader.bindVectorTexture(texture);

    if(flag == VectorGL3D::Flag{}) {
        shader.setBackgroundColor(data.backgroundColor)
            .setColor(data.color);
        if(data.textureTransformation != Matrix3{})
            shader.setTextureMatrix(data.textureTransformation);
        else shader.setTransformationProjectionMatrix(
            Matrix4::perspectiveProjection(60.0_degf, 1.0f, 0.1f, 10.0f)*
            Matrix4::translation(Vector3::zAxis(-2.15f))*
            Matrix4::rotationY(-15.0_degf)*
            Matrix4::rotationZ(15.0_degf));
        shader.draw(plane);
    }
    #ifndef MAGNUM_TARGET_GLES2
    else if(flag == VectorGL3D::Flag::UniformBuffers) {
        GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, {
            TransformationProjectionUniform3D{}
                .setTransformationProjectionMatrix(
                    data.textureTransformation == Matrix3{} ?
                        Matrix4::perspectiveProjection(60.0_degf, 1.0f, 0.1f, 10.0f)*
                        Matrix4::translation(Vector3::zAxis(-2.15f))*
                        Matrix4::rotationY(-15.0_degf)*
                        Matrix4::rotationZ(15.0_degf) : Matrix4{}
                )
        }};
        GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, {
            VectorDrawUniform{}
        }};
        GL::Buffer textureTransformationUniform{GL::Buffer::TargetHint::Uniform, {
            TextureTransformationUniform{}
                .setTextureMatrix(data.textureTransformation)
        }};
        GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, {
            VectorMaterialUniform{}
                .setBackgroundColor(data.backgroundColor)
                .setColor(data.color)
        }};
        if(data.flags & VectorGL3D::Flag::TextureTransformation)
            shader.bindTextureTransformationBuffer(textureTransformationUniform);
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindMaterialBuffer(materialUniform)
            .draw(plane);
    }
    #endif
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    MAGNUM_VERIFY_NO_GL_ERROR();

    Image2D rendered = _framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm});
    /* Dropping the alpha channel, as it's always 1.0 */
    Containers::StridedArrayView2D<Color3ub> pixels =
        Containers::arrayCast<Color3ub>(rendered.pixels<Color4ub>());
    if(data.flip) pixels = pixels.flipped<0>().flipped<1>();

    #if !(defined(MAGNUM_TARGET_GLES2) && defined(MAGNUM_TARGET_WEBGL))
    /* SwiftShader has differently rasterized edges on four pixels */
    const Float maxThreshold = 170.0f, meanThreshold = 0.171f;
    #else
    /* WebGL 1 doesn't have 8bit renderbuffer storage, so it's way worse */
    const Float maxThreshold = 170.0f, meanThreshold = 0.660f;
    #endif
    CORRADE_COMPARE_WITH(pixels,
        Utility::Path::join({_testDir, "VectorTestFiles", data.file3D}),
        (DebugTools::CompareImageToFile{_manager, maxThreshold, meanThreshold}));
}

#ifndef MAGNUM_TARGET_GLES2
void VectorGLTest::renderMulti2D() {
    auto&& data = RenderMultiData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    if(data.flags >= VectorGL2D::Flag::MultiDraw) {
        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::shader_draw_parameters>())
            CORRADE_SKIP(GL::Extensions::ARB::shader_draw_parameters::string() << "is not supported.");
        #elif !defined(MAGNUM_TARGET_WEBGL)
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ANGLE::multi_draw>())
            CORRADE_SKIP(GL::Extensions::ANGLE::multi_draw::string() << "is not supported.");
        #else
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::WEBGL::multi_draw>())
            CORRADE_SKIP(GL::Extensions::WEBGL::multi_draw::string() << "is not supported.");
        #endif
    }

    #if defined(MAGNUM_TARGET_GLES) && !defined(MAGNUM_TARGET_WEBGL)
    if(GL::Context::current().detectedDriver() & GL::Context::DetectedDriver::SwiftShader)
        CORRADE_SKIP("UBOs with dynamically indexed arrays are a crashy dumpster fire on SwiftShader, can't test.");
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    GL::Texture2D vector;
    vector.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setStorage(1, GL::TextureFormat::R8, image->size())
        .setSubImage(0, {}, *image);

    /* Circle is a fan, plane is a strip, make it indexed first */
    Trade::MeshData circleData = MeshTools::generateIndices(Primitives::circle2DSolid(32,
        Primitives::Circle2DFlag::TextureCoordinates));
    Trade::MeshData squareData = MeshTools::generateIndices(Primitives::squareSolid(
        Primitives::SquareFlag::TextureCoordinates));
    Trade::MeshData triangleData = MeshTools::generateIndices(Primitives::circle2DSolid(3,
        Primitives::Circle2DFlag::TextureCoordinates));
    GL::Mesh mesh = MeshTools::compile(MeshTools::concatenate({circleData, squareData, triangleData}));
    GL::MeshView circle{mesh};
    circle.setCount(circleData.indexCount());
    GL::MeshView square{mesh};
    square.setCount(squareData.indexCount())
        .setIndexRange(circleData.indexCount());
    GL::MeshView triangle{mesh};
    triangle.setCount(triangleData.indexCount())
        .setIndexRange(circleData.indexCount() + squareData.indexCount());

    /* Some drivers have uniform offset alignment as high as 256, which means
       the subsequent sets of uniforms have to be aligned to a multiply of it.
       The data.uniformIncrement is set high enough to ensure that, in the
       non-offset-bind case this value is 1. */

    Containers::Array<VectorMaterialUniform> materialData{data.uniformIncrement + 1};
    materialData[0*data.uniformIncrement] = VectorMaterialUniform{}
        .setColor(0xff0000_rgbf)
        .setBackgroundColor(0xffcccc_rgbf);
    materialData[1*data.uniformIncrement] = VectorMaterialUniform{}
        .setColor(0x00ff00_rgbf)
        .setBackgroundColor(0xccffcc_rgbf);
    GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, materialData};

    Containers::Array<TransformationProjectionUniform2D> transformationProjectionData{2*data.uniformIncrement + 1};
    transformationProjectionData[0*data.uniformIncrement] = TransformationProjectionUniform2D{}
        .setTransformationProjectionMatrix(
            Matrix3::projection({2.1f, 2.1f})*
            Matrix3::scaling(Vector2{0.4f})*
            Matrix3::translation({-1.25f, -1.25f})
        );
    transformationProjectionData[1*data.uniformIncrement] = TransformationProjectionUniform2D{}
        .setTransformationProjectionMatrix(
            Matrix3::projection({2.1f, 2.1f})*
            Matrix3::scaling(Vector2{0.4f})*
            Matrix3::translation({ 1.25f, -1.25f})
        );
    transformationProjectionData[2*data.uniformIncrement] = TransformationProjectionUniform2D{}
        .setTransformationProjectionMatrix(
            Matrix3::projection({2.1f, 2.1f})*
            Matrix3::scaling(Vector2{0.4f})*
            Matrix3::translation({ 0.00f,  1.25f})
        );
    GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, transformationProjectionData};

    Containers::Array<TextureTransformationUniform> textureTransformationData{2*data.uniformIncrement + 1};
    textureTransformationData[0*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(
            Matrix3::translation({0.5f, 0.5f})*
            Matrix3::rotation(180.0_degf)*
            Matrix3::translation({-0.5f, -0.5f})
        );
    textureTransformationData[1*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(
            Matrix3::translation(Vector2::xAxis(1.0f))*
            Matrix3::scaling(Vector2::xScale(-1.0f))
        );
    textureTransformationData[2*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(Matrix3{});
    GL::Buffer textureTransformationUniform{GL::Buffer::TargetHint::Uniform, textureTransformationData};

    Containers::Array<VectorDrawUniform> drawData{2*data.uniformIncrement + 1};
    /* Material offsets are zero if we have single draw, as those are done with
       UBO offset bindings instead. */
    drawData[0*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 1);
    drawData[1*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 0);
    drawData[2*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 1);
    GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, drawData};

    VectorGL2D shader{VectorGL2D::Flag::UniformBuffers|VectorGL2D::Flag::TextureTransformation|data.flags, data.materialCount, data.drawCount};
    shader.bindVectorTexture(vector);

    /* Just one draw, rebinding UBOs each time */
    if(data.drawCount == 1) {
        shader.bindMaterialBuffer(materialUniform,
            1*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            0*data.uniformIncrement*sizeof(TransformationProjectionUniform2D),
            sizeof(TransformationProjectionUniform2D));
        shader.bindDrawBuffer(drawUniform,
            0*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            0*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(circle);

        shader.bindMaterialBuffer(materialUniform,
            0*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            1*data.uniformIncrement*sizeof(TransformationProjectionUniform2D),
            sizeof(TransformationProjectionUniform2D));
        shader.bindDrawBuffer(drawUniform,
            1*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            1*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(square);

        shader.bindMaterialBuffer(materialUniform,
            1*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            2*data.uniformIncrement*sizeof(TransformationProjectionUniform2D),
            sizeof(TransformationProjectionUniform2D));
        shader.bindDrawBuffer(drawUniform,
            2*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            2*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(triangle);

    /* Otherwise using the draw offset / multidraw */
    } else {
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindTextureTransformationBuffer(textureTransformationUniform)
            .bindMaterialBuffer(materialUniform);

        if(data.flags >= VectorGL2D::Flag::MultiDraw)
            shader.draw({circle, square, triangle});
        else {
            shader.setDrawOffset(0)
                .draw(circle);
            shader.setDrawOffset(1)
                .draw(square);
            shader.setDrawOffset(2)
                .draw(triangle);
        }
    }

    /*
        -   Circle lower left, green, upside down
        -   Square lower right, red, mirrored
        -   Triangle up center, green
    */
    MAGNUM_VERIFY_NO_GL_ERROR();
    CORRADE_COMPARE_WITH(
        /* Dropping the alpha channel, as it's always 1.0 */
        Containers::arrayCast<Color3ub>(_framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm}).pixels<Color4ub>()),
        Utility::Path::join({_testDir, "VectorTestFiles", data.expected2D}),
        (DebugTools::CompareImageToFile{_manager, data.maxThreshold, data.meanThreshold}));
}

void VectorGLTest::renderMulti3D() {
    auto&& data = RenderMultiData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifndef MAGNUM_TARGET_GLES
    if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::uniform_buffer_object>())
        CORRADE_SKIP(GL::Extensions::ARB::uniform_buffer_object::string() << "is not supported.");
    #endif

    if(data.flags >= VectorGL3D::Flag::MultiDraw) {
        #ifndef MAGNUM_TARGET_GLES
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ARB::shader_draw_parameters>())
            CORRADE_SKIP(GL::Extensions::ARB::shader_draw_parameters::string() << "is not supported.");
        #elif !defined(MAGNUM_TARGET_WEBGL)
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::ANGLE::multi_draw>())
            CORRADE_SKIP(GL::Extensions::ANGLE::multi_draw::string() << "is not supported.");
        #else
        if(!GL::Context::current().isExtensionSupported<GL::Extensions::WEBGL::multi_draw>())
            CORRADE_SKIP(GL::Extensions::WEBGL::multi_draw::string() << "is not supported.");
        #endif
    }

    #if defined(MAGNUM_TARGET_GLES) && !defined(MAGNUM_TARGET_WEBGL)
    if(GL::Context::current().detectedDriver() & GL::Context::DetectedDriver::SwiftShader)
        CORRADE_SKIP("UBOs with dynamically indexed arrays are a crashy dumpster fire on SwiftShader, can't test.");
    #endif

    if(!(_manager.loadState("AnyImageImporter") & PluginManager::LoadState::Loaded) ||
       !(_manager.loadState("TgaImporter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("AnyImageImporter / TgaImporter plugins not found.");

    Containers::Pointer<Trade::AbstractImporter> importer = _manager.loadAndInstantiate("AnyImageImporter");
    CORRADE_VERIFY(importer);

    Containers::Optional<Trade::ImageData2D> image;
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(_testDir, "TestFiles/vector.tga")) && (image = importer->image2D(0)));
    GL::Texture2D vector;
    vector.setMinificationFilter(GL::SamplerFilter::Linear)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setStorage(1, GL::TextureFormat::R8, image->size())
        .setSubImage(0, {}, *image);

    Trade::MeshData sphereData = Primitives::uvSphereSolid(16, 32,
        Primitives::UVSphereFlag::TextureCoordinates);
    /* Plane is a strip, make it indexed first */
    Trade::MeshData planeData = MeshTools::generateIndices(Primitives::planeSolid(
        Primitives::PlaneFlag::TextureCoordinates));
    Trade::MeshData coneData = Primitives::coneSolid(1, 32, 1.0f,
        Primitives::ConeFlag::TextureCoordinates);
    GL::Mesh mesh = MeshTools::compile(MeshTools::concatenate({sphereData, planeData, coneData}));
    GL::MeshView sphere{mesh};
    sphere.setCount(sphereData.indexCount());
    GL::MeshView plane{mesh};
    plane.setCount(planeData.indexCount())
        .setIndexRange(sphereData.indexCount());
    GL::MeshView cone{mesh};
    cone.setCount(coneData.indexCount())
        .setIndexRange(sphereData.indexCount() + planeData.indexCount());

    /* Some drivers have uniform offset alignment as high as 256, which means
       the subsequent sets of uniforms have to be aligned to a multiply of it.
       The data.uniformIncrement is set high enough to ensure that, in the
       non-offset-bind case this value is 1. */

    Containers::Array<VectorMaterialUniform> materialData{data.uniformIncrement + 1};
    materialData[0*data.uniformIncrement] = VectorMaterialUniform{}
        .setColor(0xff0000_rgbf)
        .setBackgroundColor(0xffcccc_rgbf);
    materialData[1*data.uniformIncrement] = VectorMaterialUniform{}
        .setColor(0x00ff00_rgbf)
        .setBackgroundColor(0xccffcc_rgbf);
    GL::Buffer materialUniform{GL::Buffer::TargetHint::Uniform, materialData};

    Containers::Array<TransformationProjectionUniform3D> transformationProjectionData{2*data.uniformIncrement + 1};
    transformationProjectionData[0*data.uniformIncrement] = TransformationProjectionUniform3D{}
        .setTransformationProjectionMatrix(
            Matrix4::perspectiveProjection(60.0_degf, 1.0f, 0.1f, 10.0f)*
            Matrix4::translation(Vector3::zAxis(-2.15f))*
            Matrix4::scaling(Vector3{0.4f})*
            Matrix4::translation({-1.25f, -1.25f, 0.0f})*
            Matrix4::rotationY(180.0_degf) /* so the texture is visible */
        );
    transformationProjectionData[1*data.uniformIncrement] = TransformationProjectionUniform3D{}
        .setTransformationProjectionMatrix(
            Matrix4::perspectiveProjection(60.0_degf, 1.0f, 0.1f, 10.0f)*
            Matrix4::translation(Vector3::zAxis(-2.15f))*
            Matrix4::scaling(Vector3{0.4f})*
            Matrix4::translation({ 1.25f, -1.25f, 0.0f})
        );
    transformationProjectionData[2*data.uniformIncrement] = TransformationProjectionUniform3D{}
        .setTransformationProjectionMatrix(
            Matrix4::perspectiveProjection(60.0_degf, 1.0f, 0.1f, 10.0f)*
            Matrix4::translation(Vector3::zAxis(-2.15f))*
            Matrix4::scaling(Vector3{0.4f})*
            Matrix4::translation({  0.0f,  1.0f, 1.0f})*
            Matrix4::rotationY(180.0_degf) /* so the texture is visible */
        );
    GL::Buffer transformationProjectionUniform{GL::Buffer::TargetHint::Uniform, transformationProjectionData};

    Containers::Array<TextureTransformationUniform> textureTransformationData{2*data.uniformIncrement + 1};
    textureTransformationData[0*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(
            Matrix3::translation({0.5f, 0.5f})*
            Matrix3::rotation(180.0_degf)*
            Matrix3::translation({-0.5f, -0.5f})
        );
    textureTransformationData[1*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(
            Matrix3::translation(Vector2::xAxis(1.0f))*
            Matrix3::scaling(Vector2::xScale(-1.0f))
        );
    textureTransformationData[2*data.uniformIncrement] = TextureTransformationUniform{}
        .setTextureMatrix(Matrix3{});
    GL::Buffer textureTransformationUniform{GL::Buffer::TargetHint::Uniform, textureTransformationData};

    Containers::Array<VectorDrawUniform> drawData{2*data.uniformIncrement + 1};
    /* Material offsets are zero if we have single draw, as those are done with
       UBO offset bindings instead. */
    drawData[0*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 1);
    drawData[1*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 0);
    drawData[2*data.uniformIncrement] = VectorDrawUniform{}
        .setMaterialId(data.drawCount == 1 ? 0 : 1);
    GL::Buffer drawUniform{GL::Buffer::TargetHint::Uniform, drawData};

    VectorGL3D shader{VectorGL3D::Flag::UniformBuffers|VectorGL3D::Flag::TextureTransformation|data.flags, data.materialCount, data.drawCount};
    shader.bindVectorTexture(vector);

    /* Just one draw, rebinding UBOs each time */
    if(data.drawCount == 1) {
        shader.bindMaterialBuffer(materialUniform,
            1*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            0*data.uniformIncrement*sizeof(TransformationProjectionUniform3D),
            sizeof(TransformationProjectionUniform3D));
        shader.bindDrawBuffer(drawUniform,
            0*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            0*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(sphere);

        shader.bindMaterialBuffer(materialUniform,
            0*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            1*data.uniformIncrement*sizeof(TransformationUniform3D),
            sizeof(TransformationUniform3D));
        shader.bindDrawBuffer(drawUniform,
            1*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            1*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(plane);

        shader.bindMaterialBuffer(materialUniform,
            1*data.uniformIncrement*sizeof(VectorMaterialUniform),
            sizeof(VectorMaterialUniform));
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform,
            2*data.uniformIncrement*sizeof(TransformationUniform3D),
            sizeof(TransformationUniform3D));
        shader.bindDrawBuffer(drawUniform,
            2*data.uniformIncrement*sizeof(VectorDrawUniform),
            sizeof(VectorDrawUniform));
        shader.bindTextureTransformationBuffer(textureTransformationUniform,
            2*data.uniformIncrement*sizeof(TextureTransformationUniform),
            sizeof(TextureTransformationUniform));
        shader.draw(cone);

    /* Otherwise using the draw offset / multidraw */
    } else {
        shader.bindTransformationProjectionBuffer(transformationProjectionUniform)
            .bindDrawBuffer(drawUniform)
            .bindTextureTransformationBuffer(textureTransformationUniform)
            .bindMaterialBuffer(materialUniform);

        if(data.flags >= VectorGL3D::Flag::MultiDraw)
            shader.draw({sphere, plane, cone});
        else {
            shader.setDrawOffset(0)
                .draw(sphere);
            shader.setDrawOffset(1)
                .draw(plane);
            shader.setDrawOffset(2)
                .draw(cone);
        }
    }

    /*
        -   Sphere lower left, green, upside down
        -   Plane lower right, red, mirrored
        -   Cone up center, green
    */
    MAGNUM_VERIFY_NO_GL_ERROR();
    CORRADE_COMPARE_WITH(
        /* Dropping the alpha channel, as it's always 1.0 */
        Containers::arrayCast<Color3ub>(_framebuffer.read(_framebuffer.viewport(), {PixelFormat::RGBA8Unorm}).pixels<Color4ub>()),
        Utility::Path::join({_testDir, "VectorTestFiles", data.expected3D}),
        (DebugTools::CompareImageToFile{_manager, data.maxThreshold, data.meanThreshold}));
}
#endif

}}}}

CORRADE_TEST_MAIN(Magnum::Shaders::Test::VectorGLTest)
