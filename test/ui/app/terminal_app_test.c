// determinant/test/ui/app/terminal_app_test.c
// Owner: TerminalApp — host lifecycle harness, d64-buffered rendering,
// and step-driven presentation.

#include "ui/app/terminal_app.h"
#include "ui/panel/terminal_panel.h"
#include "ui/text/terminal_text.h"
#include "determinant/test/support.h"
#include <unistd.h>
#include <fcntl.h>

static void testHeadlessLifecycle(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    assert(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);

    TerminalPanel *root = TerminalPanel_0();
    assert(root != nullptr);

    TerminalText *label = TerminalText("Vexgraph Terminal App");
    assert(label != nullptr);
    TerminalChildId child = TerminalPanel_addText(root, label, TerminalAnchor_0());
    assert(child.generation != 0);

    TerminalApp *app = TerminalApp(root);
    assert(app != nullptr);
    assert(TerminalApp_getRoot(app) == root);

    TerminalCompositor *comp = TerminalCompositor(fds[1]);
    assert(comp != nullptr);
    assert(TerminalApp_setCompositor(app, comp));

    assert(TerminalApp_setHeadless(app, true));
    assert(TerminalApp_isHeadless(app));

    assert(TerminalApp_resize(app, 40, 10));
    assert(TerminalApp_getWidth(app) == 40);
    assert(TerminalApp_getHeight(app) == 10);

    // Step a frame
    assert(TerminalApp_step(app));

    char buf[2048];
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, "Vexgraph Terminal App") != nullptr);

    // Feed mouse input
    assert(TerminalApp_feedInput(app, "\033[<0;10;5M", 9));

    // Run in headless mode (completes immediately)
    assert(TerminalApp_run(app) == 0);

    TERMINAL_PROJECT(TerminalApp, app);

    TerminalPanel_free(root);
    TerminalApp_free(app);
    close(fds[0]);
    close(fds[1]);
}

static void testNegativeAppRejections(void) {
    assert(!TerminalApp_setRoot(nullptr, nullptr));
    assert(!TerminalApp_setCompositor(nullptr, nullptr));
    assert(!TerminalApp_resize(nullptr, 0, 0));
    assert(!TerminalApp_step(nullptr));
    assert(!TerminalApp_feedInput(nullptr, nullptr, 0));

    TerminalApp *app = TerminalApp_0();
    assert(app != nullptr);
    assert(!TerminalApp_step(app)); // no root
    assert(TerminalApp_run(app) != 0); // no root

    TerminalApp_free(app);
}

int main(void) {
    testHeadlessLifecycle();
    testNegativeAppRejections();
    return 0;
}
