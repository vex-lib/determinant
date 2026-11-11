// Owner: TerminalBreadcrumb — bounded hierarchical navigation breadcrumb with arrow separator.
#include "ui/text/terminal_breadcrumb.h"
#include "determinant/test/support.h"

int main(void) {
    enum { W = 40, H = 1 };
    static u32 cells[W * H];
    static u32 fg[W * H];
    TerminalCanvas wide = TerminalCanvas(cells, nullptr, fg, nullptr, W, H);
    TerminalCanvas_fill(&wide, ' ', TerminalPen());

    // 1. Default constructor and separator
    TerminalBreadcrumb breadcrumb = TerminalBreadcrumb_0();
    assert(TerminalBreadcrumb_getRoot(&breadcrumb) == nullptr);
    assert(TerminalBreadcrumb_getPath(&breadcrumb) == nullptr);
    assert(strcmp(TerminalBreadcrumb_getSeparator(&breadcrumb), " › ") == 0);
    assert(TerminalBreadcrumb_getMaxCells(&breadcrumb) == 0u);

    // 2. Setters & Getters
    assert(TerminalBreadcrumb_setRoot(&breadcrumb, "/workspace/project"));
    assert(TerminalBreadcrumb_setPath(&breadcrumb, "/workspace/project", "/workspace/project/src/main.c"));
    assert(strcmp(TerminalBreadcrumb_getRoot(&breadcrumb), "/workspace/project") == 0);
    assert(strcmp(TerminalBreadcrumb_getPath(&breadcrumb), "/workspace/project/src/main.c") == 0);

    TerminalPen testPen = TerminalPen(0, TERMINAL_RGB(0x7A, 0xC8, 0xFF));
    TerminalBreadcrumb_setPen(&breadcrumb, testPen);
    assert(TerminalBreadcrumb_getPen(&breadcrumb).fg == TERMINAL_RGB(0x7A, 0xC8, 0xFF));

    // 3. Shallow path inside root (<= 4 segments)
    char buffer[256];
    bytes length = TerminalBreadcrumb_format(&breadcrumb, buffer, sizeof buffer);
    assert(length > 0u);
    assert(strcmp(buffer, "project › src › main.c") == 0);

    // 4. Outside root
    TerminalBreadcrumb outsideBreadcrumb = TerminalBreadcrumb_2(
        "/workspace/project", "suite/ecosystem/vexspoke/src/oop/type.h");
    length = TerminalBreadcrumb_format(&outsideBreadcrumb, buffer, sizeof buffer);
    assert(length > 0u);
    assert(strcmp(buffer, "… › oop › type.h") == 0);

    // 5. Deep path inside root (> 4 segments) collapses interior
    TerminalBreadcrumb deepBreadcrumb = TerminalBreadcrumb_2(
        "/workspace/project",
        "/workspace/project/suite/projects/semicolon/src/tui/v2/filetree.c");
    length = TerminalBreadcrumb_format(&deepBreadcrumb, buffer, sizeof buffer);
    assert(length > 0u);
    assert(strcmp(buffer, "project › suite › … › v2 › filetree.c") == 0);

    // 6. Max cells budgeting & measure
    TerminalBreadcrumb budgetBreadcrumb = TerminalBreadcrumb_4(
        "/workspace/project",
        "/workspace/project/suite/projects/semicolon/src/tui/v2/filetree.c",
        " › ", 20u);
    assert(TerminalBreadcrumb_getMaxCells(&budgetBreadcrumb) == 20u);
    length = TerminalBreadcrumb_format(&budgetBreadcrumb, buffer, sizeof buffer);
    assert(length > 0u);
    bytes measuredCells = TerminalBreadcrumb_measure(&budgetBreadcrumb);
    assert(measuredCells <= 20u);

    // 7. Render to TerminalCanvas
    TerminalBreadcrumb_render(&outsideBreadcrumb, &wide, 0u, 0u);
    // First character is '…' (U+2026)
    assert(TerminalCanvas_glyphAt(&wide, 0u, 0u) == 0x2026u);

    // 8. Custom separator
    TerminalBreadcrumb customSep = TerminalBreadcrumb_3("root", "root/a/b.sh.c", " > ");
    length = TerminalBreadcrumb_format(&customSep, buffer, sizeof buffer);
    assert(strcmp(buffer, "root > a > b.sh.c") == 0);

    // 9. Null safety and invalid self rejection
    TerminalBreadcrumb_render(nullptr, &wide, 0u, 0u);
    TerminalBreadcrumb_render(&breadcrumb, nullptr, 0u, 0u);
    assert(TerminalBreadcrumb_getRoot(nullptr) == nullptr);
    assert(TerminalBreadcrumb_getPath(nullptr) == nullptr);
    assert(TerminalBreadcrumb_getSeparator(nullptr) == nullptr);
    assert(TerminalBreadcrumb_getMaxCells(nullptr) == 0u);
    TerminalBreadcrumb_setPen(nullptr, TerminalPen_0());

    REJECT(TerminalBreadcrumb_setPath(nullptr, "root", "path"), "null self");
    REJECT(TerminalBreadcrumb_setRoot(nullptr, "root"), "null self");
    REJECT(TerminalBreadcrumb_setSeparator(nullptr, " › "), "null self");
    REJECT(TerminalBreadcrumb_setMaxCells(nullptr, 10u), "null self");

    // 10. Projections
    TERMINAL_PROJECT(TerminalBreadcrumb, &breadcrumb);
    return 0;
}
