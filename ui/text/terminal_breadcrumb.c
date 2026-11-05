#include "ui/text/terminal_breadcrumb.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/annotation.h"

#include <stdio.h>
#include <string.h>

;;DEFINITION
/** Bounded hierarchical navigation breadcrumb for terminal surfaces.
 * Formats file/folder trails, collapses interior depth into ellipses ("…")
 * and applies cell constraints using the arrow separator (" › "). */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalBreadcrumb (ui/text/terminal_breadcrumb.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_breadcrumb.h)
 *   const char *root       borrowed root directory or project label (nullable)
 *   const char *path       borrowed target file path
 *   const char *separator  borrowed arrow separator (default " › ")
 *   size_t maxCells        horizontal column budget (0 = unconstrained)
 *   TerminalPen pen        presentation style and color
 *
 * FUNCTION REGISTRY
 *   Constructors: TerminalBreadcrumb_0/1/2/3/4
 *   Setters: setPath, setRoot, setSeparator, setMaxCells, setPen
 *   Getters: getRoot, getPath, getSeparator, getMaxCells, getPen
 *   Operations: format, measure, render
 *   Projections: toString, toStringStruct
 * ======================================================================== */

TerminalBreadcrumb TerminalBreadcrumb_0(void) {
    TerminalBreadcrumb breadcrumbInstance = {0};
    breadcrumbInstance.separator = TERMINAL_BREADCRUMB_SEPARATOR_DEFAULT;
    breadcrumbInstance.pen = TerminalPen_0();
    return breadcrumbInstance;
}

TerminalBreadcrumb TerminalBreadcrumb_1(const char *targetPath) {
    TerminalBreadcrumb breadcrumbInstance = TerminalBreadcrumb_0();
    breadcrumbInstance.path = targetPath;
    return breadcrumbInstance;
}

TerminalBreadcrumb TerminalBreadcrumb_2(const char *rootDirectory, const char *targetPath) {
    TerminalBreadcrumb breadcrumbInstance = TerminalBreadcrumb_0();
    breadcrumbInstance.root = rootDirectory;
    breadcrumbInstance.path = targetPath;
    return breadcrumbInstance;
}

TerminalBreadcrumb TerminalBreadcrumb_3(const char *rootDirectory, const char *targetPath,
                                        const char *customSeparator) {
    TerminalBreadcrumb breadcrumbInstance = TerminalBreadcrumb_2(rootDirectory, targetPath);
    if (customSeparator != nullptr)
        breadcrumbInstance.separator = customSeparator;
    return breadcrumbInstance;
}

TerminalBreadcrumb TerminalBreadcrumb_4(const char *rootDirectory, const char *targetPath,
                                        const char *customSeparator, size_t maximumCells) {
    TerminalBreadcrumb breadcrumbInstance = TerminalBreadcrumb_3(rootDirectory, targetPath, customSeparator);
    breadcrumbInstance.maxCells = maximumCells;
    return breadcrumbInstance;
}

bool TerminalBreadcrumb_setPath(TerminalBreadcrumb *self, const char *rootDirectory, const char *targetPath) {
    if (self == nullptr) {
        THROW("TerminalBreadcrumb setPath: null self");
        return false;
    }
    (*self).root = rootDirectory;
    (*self).path = targetPath;
    return true;
}

bool TerminalBreadcrumb_setRoot(TerminalBreadcrumb *self, const char *rootDirectory) {
    if (self == nullptr) {
        THROW("TerminalBreadcrumb setRoot: null self");
        return false;
    }
    (*self).root = rootDirectory;
    return true;
}

bool TerminalBreadcrumb_setSeparator(TerminalBreadcrumb *self, const char *customSeparator) {
    if (self == nullptr) {
        THROW("TerminalBreadcrumb setSeparator: null self");
        return false;
    }
    (*self).separator = customSeparator != nullptr ? customSeparator : TERMINAL_BREADCRUMB_SEPARATOR_DEFAULT;
    return true;
}

bool TerminalBreadcrumb_setMaxCells(TerminalBreadcrumb *self, size_t maximumCells) {
    if (self == nullptr) {
        THROW("TerminalBreadcrumb setMaxCells: null self");
        return false;
    }
    (*self).maxCells = maximumCells;
    return true;
}

void TerminalBreadcrumb_setPen(TerminalBreadcrumb *self, TerminalPen textPen) {
    if (self != nullptr)
        (*self).pen = textPen;
}

const char *TerminalBreadcrumb_getRoot(const TerminalBreadcrumb *self) {
    return self != nullptr ? (*self).root : nullptr;
}

const char *TerminalBreadcrumb_getPath(const TerminalBreadcrumb *self) {
    return self != nullptr ? (*self).path : nullptr;
}

const char *TerminalBreadcrumb_getSeparator(const TerminalBreadcrumb *self) {
    return self != nullptr ? (*self).separator : nullptr;
}

size_t TerminalBreadcrumb_getMaxCells(const TerminalBreadcrumb *self) {
    return self != nullptr ? (*self).maxCells : 0u;
}

TerminalPen TerminalBreadcrumb_getPen(const TerminalBreadcrumb *self) {
    return self != nullptr ? (*self).pen : TerminalPen_0();
}

static size_t TerminalBreadcrumb_measureStringCells(const char *byteSpan, size_t byteLength) {
    if (byteSpan == nullptr || byteLength == 0u)
        return 0u;
    size_t totalCells = 0u;
    size_t byteOffset = 0u;
    while (byteOffset < byteLength) {
        TerminalCanvasDecodedScalar decodedScalar = TerminalCanvas_utf8Decode(byteSpan + byteOffset,
                                                                             byteLength - byteOffset);
        if (decodedScalar.bytes == 0u) {
            ++totalCells;
            ++byteOffset;
            continue;
        }
        totalCells += TerminalCanvas_widthOf(decodedScalar.codepoint);
        byteOffset += decodedScalar.bytes;
    }
    return totalCells;
}

static size_t TerminalBreadcrumb_bytesForCellBudget(const char *byteSpan, size_t byteLength,
                                                    size_t cellBudget) {
    if (byteSpan == nullptr || byteLength == 0u || cellBudget == 0u)
        return 0u;
    size_t consumedCells = 0u;
    size_t byteOffset = 0u;
    while (byteOffset < byteLength && consumedCells < cellBudget) {
        TerminalCanvasDecodedScalar decodedScalar = TerminalCanvas_utf8Decode(byteSpan + byteOffset,
                                                                             byteLength - byteOffset);
        if (decodedScalar.bytes == 0u) {
            ++consumedCells;
            ++byteOffset;
            continue;
        }
        size_t cellWidth = TerminalCanvas_widthOf(decodedScalar.codepoint);
        if (consumedCells + cellWidth > cellBudget)
            break;
        consumedCells += cellWidth;
        byteOffset += decodedScalar.bytes;
    }
    return byteOffset;
}

size_t TerminalBreadcrumb_format(const TerminalBreadcrumb *self, char *destinationBuffer,
                                 size_t destinationCapacity) {
    if (self == nullptr || destinationBuffer == nullptr || destinationCapacity == 0u)
        return 0u;

    destinationBuffer[0] = '\0';
    if ((*self).path == nullptr || (*self).path[0] == '\0')
        return 0u;

    const char *activeSeparator = (*self).separator != nullptr ? (*self).separator
                                                               : TERMINAL_BREADCRUMB_SEPARATOR_DEFAULT;
    const char *rootDirectory = (*self).root;
    bool isInsideRoot = false;
    const char *relativeCursor = (*self).path;
    const char *rootBaseName = "";

    if (rootDirectory != nullptr && rootDirectory[0] != '\0') {
        size_t rootLength = strlen(rootDirectory);
        if (strncmp((*self).path, rootDirectory, rootLength) == 0
            && ((*self).path[rootLength] == '/' || (*self).path[rootLength] == '\0')) {
            isInsideRoot = true;
            relativeCursor = (*self).path[rootLength] == '/' ? (*self).path + rootLength + 1u : "";
        }
        const char *slashPointer = strrchr(rootDirectory, '/');
        rootBaseName = (slashPointer != nullptr && *(slashPointer + 1u) != '\0') ? slashPointer + 1u
                                                                                : rootDirectory;
        if (*rootBaseName == '\0')
            rootBaseName = "/";
    }

    const char *firstSegmentPointers[2] = { nullptr, nullptr };
    const char *lastSegmentPointers[2] = { nullptr, nullptr };
    size_t firstSegmentLengths[2] = { 0u, 0u };
    size_t lastSegmentLengths[2] = { 0u, 0u };
    size_t segmentCount = 0u;

    const char *scanPointer = relativeCursor;
    while (*scanPointer != '\0') {
        const char *endPointer = strchr(scanPointer, '/');
        size_t segmentLength = endPointer == nullptr ? strlen(scanPointer)
                                                     : (size_t) (endPointer - scanPointer);
        if (segmentLength != 0u) {
            if (segmentCount < 2u) {
                firstSegmentPointers[segmentCount] = scanPointer;
                firstSegmentLengths[segmentCount] = segmentLength;
            }
            lastSegmentPointers[0] = lastSegmentPointers[1];
            lastSegmentLengths[0] = lastSegmentLengths[1];
            lastSegmentPointers[1] = scanPointer;
            lastSegmentLengths[1] = segmentLength;
            ++segmentCount;
        }
        if (endPointer == nullptr)
            break;
        scanPointer = endPointer + 1u;
    }

    char scratchBuffer[1024];
    scratchBuffer[0] = '\0';

    if (!isInsideRoot) {
        if (segmentCount >= 2u) {
            (void) snprintf(scratchBuffer, sizeof scratchBuffer, "…%s%.*s%s%.*s",
                            activeSeparator,
                            (int) lastSegmentLengths[0], lastSegmentPointers[0],
                            activeSeparator,
                            (int) lastSegmentLengths[1], lastSegmentPointers[1]);
        } else if (segmentCount == 1u) {
            (void) snprintf(scratchBuffer, sizeof scratchBuffer, "…%s%.*s",
                            activeSeparator,
                            (int) lastSegmentLengths[1], lastSegmentPointers[1]);
        } else {
            (void) snprintf(scratchBuffer, sizeof scratchBuffer, "…");
        }
    } else {
        if (segmentCount == 0u) {
            (void) snprintf(scratchBuffer, sizeof scratchBuffer, "%s", rootBaseName);
        } else if (segmentCount <= 4u) {
            size_t writtenBytes = (size_t) snprintf(scratchBuffer, sizeof scratchBuffer, "%s", rootBaseName);
            const char *componentScanner = relativeCursor;
            while (*componentScanner != '\0' && writtenBytes + 2u < sizeof scratchBuffer) {
                const char *nextSlash = strchr(componentScanner, '/');
                size_t componentLength = nextSlash == nullptr ? strlen(componentScanner)
                                                              : (size_t) (nextSlash - componentScanner);
                if (componentLength != 0u) {
                    writtenBytes += (size_t) snprintf(scratchBuffer + writtenBytes,
                                                     sizeof scratchBuffer - writtenBytes,
                                                     "%s%.*s", activeSeparator,
                                                     (int) componentLength, componentScanner);
                }
                if (nextSlash == nullptr)
                    break;
                componentScanner = nextSlash + 1u;
            }
        } else {
            (void) snprintf(scratchBuffer, sizeof scratchBuffer, "%s%s%.*s%s…%s%.*s%s%.*s",
                            rootBaseName, activeSeparator,
                            (int) firstSegmentLengths[0], firstSegmentPointers[0],
                            activeSeparator, activeSeparator,
                            (int) lastSegmentLengths[0], lastSegmentPointers[0],
                            activeSeparator,
                            (int) lastSegmentLengths[1], lastSegmentPointers[1]);
        }
    }

    size_t rawByteLength = strlen(scratchBuffer);
    if ((*self).maxCells > 0u) {
        size_t measuredCells = TerminalBreadcrumb_measureStringCells(scratchBuffer, rawByteLength);
        if (measuredCells > (*self).maxCells) {
            size_t keepBudget = (*self).maxCells > 1u ? (*self).maxCells - 1u : 0u;
            size_t byteCut = TerminalBreadcrumb_bytesForCellBudget(scratchBuffer, rawByteLength, keepBudget);
            if (byteCut + 4u < sizeof scratchBuffer) {
                memcpy(scratchBuffer + byteCut, "…", 3u);
                scratchBuffer[byteCut + 3u] = '\0';
                rawByteLength = byteCut + 3u;
            }
        }
    }

    if (rawByteLength >= destinationCapacity)
        rawByteLength = destinationCapacity - 1u;
    memcpy(destinationBuffer, scratchBuffer, rawByteLength);
    destinationBuffer[rawByteLength] = '\0';
    return rawByteLength;
}

size_t TerminalBreadcrumb_measure(const TerminalBreadcrumb *self) {
    if (self == nullptr)
        return 0u;
    char measureBuffer[1024];
    size_t formattedBytes = TerminalBreadcrumb_format(self, measureBuffer, sizeof measureBuffer);
    return TerminalBreadcrumb_measureStringCells(measureBuffer, formattedBytes);
}

void TerminalBreadcrumb_render(const TerminalBreadcrumb *self, TerminalCanvas *canvas,
                               size_t xCoordinate, size_t yCoordinate) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    char renderBuffer[1024];
    size_t formattedBytes = TerminalBreadcrumb_format(self, renderBuffer, sizeof renderBuffer);
    if (formattedBytes == 0u)
        return;
    TerminalCanvas_text(canvas, xCoordinate, yCoordinate, renderBuffer, formattedBytes, (*self).pen);
}

TerminalProjection TerminalBreadcrumb_toString(const TerminalBreadcrumb *self) {
    return TerminalBreadcrumb_toStringStruct(self);
}

TerminalProjection TerminalBreadcrumb_toStringStruct(const TerminalBreadcrumb *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalBreadcrumb{root=\"%s\",path=\"%s\",separator=\"%s\",maxCells=%zu}",
        (*self).root != nullptr ? (*self).root : "",
        (*self).path != nullptr ? (*self).path : "",
        (*self).separator != nullptr ? (*self).separator : "",
        (*self).maxCells);
}
