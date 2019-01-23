/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>

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

#include "Magnum/Platform/Sdl2Application.h"

#include <SDL_events.h>

namespace Magnum { namespace Platform { namespace Test { namespace {

struct Sdl2ApplicationTest: Platform::Application {
    /* For testing resize events */
    explicit Sdl2ApplicationTest(const Arguments& arguments): Platform::Application{arguments, Configuration{}.setWindowFlags(Configuration::WindowFlag::Resizable)} {}

    void exitEvent(ExitEvent& event) override {
        Debug{} << "application exiting";
        event.setAccepted(); /* Comment-out to test app exit suppression */
    }

    void drawEvent() override {}

    /* For testing HiDPI resize events */
    void viewportEvent(ViewportEvent& event) override {
        Debug{} << "viewport event" << event.windowSize() << event.framebufferSize() << event.dpiScaling();
    }

    /* For testing event coordinates */
    void mousePressEvent(MouseEvent& event) override {
        Debug{} << event.position();
    }

    /* For testing keyboard capture */
    void keyPressEvent(KeyEvent& event) override {
        Debug{} << event.keyName();
    }

    /* Should fire on currently not handled events, such as minimize/maximize.
       Comment out to verify correct behavior with the override not present. */
    void anyEvent(SDL_Event& event) override {
        Debug{} << "any event" << event.type;
    }
};

}}}}

MAGNUM_APPLICATION_MAIN(Magnum::Platform::Test::Sdl2ApplicationTest)
