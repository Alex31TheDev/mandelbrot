const fs = require("fs");
const path = require("path");
const { parseArgs: parseNodeArgs } = require("util");
const {
    ExtensionGroups,
    FileUtil,
    FormatterUtil,
    confirmRepoRoot,
    getTargetFiles,
    getRepoRoot,
    isHeaderFile
} = require("./util/formatter-util");

const defaultSourceRoots = ["mandelbrot-gui"],
    defaultIncludeSearchRoots = [
        "mandelbrot-gui",
        "common/src",
        "frontend/src",
        "mandelbrot/src",
        "mandelbrot-cli/src"
    ];

const IncludeGroups = Object.freeze({
    stl: "stl",
    win32: "win32",
    qt: "qt",
    backend: "backend",
    external: "external",
    app: "app",
    runtime: "runtime",
    controllers: "controllers",
    services: "services",
    settings: "settings",
    locale: "locale",
    windows: "windows",
    dialogs: "dialogs",
    widgets: "widgets",
    util: "util"
});

const IncludeRules = {
    guiRootName: "mandelbrot-gui",

    groupOrder: [
        IncludeGroups.stl,
        IncludeGroups.win32,
        IncludeGroups.qt,
        IncludeGroups.backend,
        IncludeGroups.external,
        IncludeGroups.app,
        IncludeGroups.runtime,
        IncludeGroups.controllers,
        IncludeGroups.services,
        IncludeGroups.settings,
        IncludeGroups.locale,
        IncludeGroups.windows,
        IncludeGroups.dialogs,
        IncludeGroups.widgets,
        IncludeGroups.util
    ],

    pathGroups: {
        app: IncludeGroups.app,
        runtime: IncludeGroups.runtime,
        controllers: IncludeGroups.controllers,
        services: IncludeGroups.services,
        settings: IncludeGroups.settings,
        locale: IncludeGroups.locale,
        windows: IncludeGroups.windows,
        dialogs: IncludeGroups.dialogs,
        widgets: IncludeGroups.widgets,
        util: IncludeGroups.util
    },

    weightOrder: {
        [IncludeGroups.app]: ["GUIAppController", "GUISessionState", "GUITypes", "GUIConstants"],
        [IncludeGroups.services]: ["point", "palette", "sine"],
        [IncludeGroups.settings]: ["AppSettings", "Shortcuts"]
    },

    windowsSystemHeaders: new Set([
        "commctrl.h",
        "commdlg.h",
        "combaseapi.h",
        "objbase.h",
        "shlobj.h",
        "shlwapi.h",
        "shobjidl.h",
        "shellapi.h",
        "unknwn.h",
        "windows.h",
        "winuser.h"
    ]),

    backendInclude: "BackendAPI.h",
    backendUsing: "using namespace Backend;",

    guiUsing: "using namespace GUI;"
};

let args = null;

const usage = `Usage: node ./tools/format-includes.js [paths...] [options]

Options:
  --repo-root <path>        Repository root
  --source-roots <items>    Comma-separated source roots
  --paths <items>           Comma-separated files or directories
  -y                        Trust the deduced repo root
  --check                   Report files that would change and exit 1
  --help                    Show this help`;

const Util = Object.freeze({
    isBlankLine: line => {
        return line.trim() === "";
    },

    isCommentOnlyLine: line => {
        return /^\s*(?:\/\/.*|\/\*.*|\*.*|\*\/)\s*$/.test(line);
    },

    trimStartBlankLines: lines => {
        let index = 0;

        while (index < lines.length && Util.isBlankLine(lines[index])) {
            index += 1;
        }

        return lines.slice(index);
    },

    trimEndBlankLines: lines => {
        let index = lines.length;

        while (index > 0 && Util.isBlankLine(lines[index - 1])) {
            index -= 1;
        }

        return lines.slice(0, index);
    },

    getLeafName: includePath => {
        return path.posix.basename(FormatterUtil.normalizeSlashes(includePath));
    },

    getLeafStem: includePath => {
        const leafName = Util.getLeafName(includePath),
            extension = path.posix.extname(leafName);

        return extension ? leafName.slice(0, -extension.length) : leafName;
    },

    makeAbsoluteUniquePaths: entries => {
        const seen = new Set(),
            output = [];

        for (const entry of entries) {
            const resolved = path.resolve(entry),
                normalized = FormatterUtil.normalizeSlashes(resolved).toLowerCase();

            if (seen.has(normalized)) continue;

            seen.add(normalized);
            output.push(resolved);
        }

        return output;
    },

    samePath: (left, right) => {
        return (
            FormatterUtil.normalizeSlashes(path.resolve(left)).toLowerCase() ===
            FormatterUtil.normalizeSlashes(path.resolve(right)).toLowerCase()
        );
    }
});

function findContainingProjectRoot(filePath) {
    let current = path.resolve(path.dirname(filePath));

    while (true) {
        if (path.basename(current).toLowerCase() === IncludeRules.guiRootName) {
            return current;
        }

        const parent = path.dirname(current);
        if (parent === current) return null;

        current = parent;
    }
}

function parseIncludeLine(line) {
    const match = line.match(/^(\s*)#\s*include\s*([<"])([^>"]+)([>"])\s*$/);
    if (!match) return null;

    return {
        indent: match[1] ?? "",
        delimiter: match[2],
        closingDelimiter: match[4],
        includePath: FormatterUtil.normalizeSlashes(match[3]),
        line
    };
}

function isUsingNamespaceLine(line, namespaceName) {
    return new RegExp(`^\\s*using\\s+namespace\\s+${namespaceName}\\s*;\\s*$`).test(line);
}

function isConditionalStart(line) {
    return /^\s*#\s*(?:if|ifdef|ifndef)\b/.test(line);
}

function isConditionalEnd(line) {
    return /^\s*#\s*endif\b/.test(line);
}

function parseConditionalBlock(lines, startIndex) {
    let depth = 0,
        index = startIndex,
        blockLines = [];

    while (index < lines.length) {
        const line = lines[index];
        blockLines.push(line);

        if (isConditionalStart(line)) {
            depth += 1;
        } else if (isConditionalEnd(line)) {
            depth -= 1;

            if (depth === 0) {
                break;
            }
        }

        index += 1;
    }

    if (depth !== 0) return null;

    return {
        type: "conditional",
        endIndex: index,
        lines: blockLines
    };
}

function getFileInfo(filePath) {
    const normalizedFilePath = FormatterUtil.normalizeSlashes(filePath),
        leafName = path.posix.basename(normalizedFilePath),
        extension = path.posix.extname(leafName),
        stem = extension ? leafName.slice(0, -extension.length) : leafName;

    return {
        filePath: path.resolve(filePath),
        directory: path.resolve(path.dirname(filePath)),
        leafName,
        stem,
        isHeader: isHeaderFile(filePath),
        localProjectRoot: findContainingProjectRoot(filePath)
    };
}

function getIncludeSearchRoots(fileInfo) {
    const roots = [];

    if (fileInfo.localProjectRoot) {
        roots.push(fileInfo.localProjectRoot);
    }

    roots.push(...args.includeSearchRoots);
    return Util.makeAbsoluteUniquePaths(roots);
}

function shouldShortenSameFolderInclude(fileInfo, includePath) {
    if (!includePath.includes("/")) return false;

    const candidateRoots = getIncludeSearchRoots(fileInfo);

    for (const rootPath of candidateRoots) {
        const candidatePath = path.resolve(rootPath, includePath);

        if (!fs.existsSync(candidatePath)) continue;
        if (Util.samePath(path.dirname(candidatePath), fileInfo.directory)) return true;
    }

    const localCandidatePath = path.resolve(fileInfo.directory, includePath);

    if (fs.existsSync(localCandidatePath)) {
        return Util.samePath(path.dirname(localCandidatePath), fileInfo.directory);
    }

    return false;
}

function buildIncludeLine(delimiter, includePath) {
    const closingDelimiter = delimiter === "<" ? ">" : '"';
    return `#include ${delimiter}${includePath}${closingDelimiter}`;
}

function normalizeIncludePath(fileInfo, includeInfo, forceBasename = false) {
    let normalizedPath = FormatterUtil.normalizeSlashes(includeInfo.includePath);

    if (includeInfo.delimiter === '"' && (forceBasename || shouldShortenSameFolderInclude(fileInfo, normalizedPath))) {
        normalizedPath = Util.getLeafName(normalizedPath);
    }

    return normalizedPath;
}

function normalizeConditionalLines(fileInfo, lines) {
    return lines.map(line => {
        const includeInfo = parseIncludeLine(line);
        if (!includeInfo) return line;

        const normalizedIncludePath = normalizeIncludePath(fileInfo, includeInfo);
        return buildIncludeLine(includeInfo.delimiter, normalizedIncludePath);
    });
}

function getOwnHeaderNames(fileInfo) {
    return [...ExtensionGroups.headerExtensions].map(extension => `${fileInfo.stem}${extension}`);
}

function isOwnHeaderInclude(fileInfo, includeInfo) {
    if (fileInfo.isHeader) return false;

    const ownHeaderNames = new Set(getOwnHeaderNames(fileInfo).map(entry => entry.toLowerCase()));
    return ownHeaderNames.has(Util.getLeafName(includeInfo.includePath).toLowerCase());
}

function isUIInclude(includeInfo) {
    return Util.getLeafName(includeInfo.includePath).startsWith("ui_");
}

function isQtInclude(includePath) {
    return /^Q[A-Z]/.test(includePath) || /^Qt[A-Z]/.test(includePath);
}

function isWindowsSystemInclude(includePath) {
    return IncludeRules.windowsSystemHeaders.has(Util.getLeafName(includePath).toLowerCase());
}

function isBackendApiInclude(includePath) {
    return Util.getLeafName(includePath).toLowerCase() === IncludeRules.backendInclude.toLowerCase();
}

function getIncludeGroup(includeInfo) {
    if (isBackendApiInclude(includeInfo.includePath)) {
        return IncludeGroups.backend;
    }

    if (includeInfo.delimiter === "<") {
        if (isWindowsSystemInclude(includeInfo.includePath)) return IncludeGroups.win32;
        if (isQtInclude(includeInfo.includePath)) return IncludeGroups.qt;
        return IncludeGroups.stl;
    }

    const firstSegment = FormatterUtil.normalizeSlashes(includeInfo.includePath).split("/")[0];
    return IncludeRules.pathGroups[firstSegment] ?? IncludeGroups.external;
}

function getConditionalGroup(entry) {
    const groups = new Set();

    for (const line of entry.lines) {
        const includeInfo = parseIncludeLine(line);
        if (!includeInfo) continue;

        groups.add(getIncludeGroup(includeInfo));
    }

    if (groups.size === 1) {
        return [...groups][0];
    }

    if (groups.size > 1) {
        return IncludeGroups.external;
    }

    return IncludeGroups.external;
}

function getWeight(groupId, sortKey) {
    const rules = IncludeRules.weightOrder[groupId];
    if (!rules) return Number.MAX_SAFE_INTEGER;

    const loweredSortKey = sortKey.toLowerCase();

    for (let i = 0; i < rules.length; i += 1) {
        const rule = rules[i];

        if (groupId === IncludeGroups.services) {
            if (loweredSortKey.includes(rule.toLowerCase())) return i;
        } else if (Util.getLeafStem(sortKey).toLowerCase() === rule.toLowerCase()) {
            return i;
        }
    }

    return Number.MAX_SAFE_INTEGER;
}

function getEntrySortKey(entry) {
    if (entry.type === "include") {
        return FormatterUtil.normalizeSlashes(entry.includePath).toLowerCase();
    }

    for (const line of entry.lines) {
        const includeInfo = parseIncludeLine(line);
        if (includeInfo) {
            return FormatterUtil.normalizeSlashes(includeInfo.includePath).toLowerCase();
        }
    }

    return entry.lines.join("\n").toLowerCase();
}

function sortEntries(groupId, entries) {
    return [...entries].sort((left, right) => {
        const leftKey = getEntrySortKey(left),
            rightKey = getEntrySortKey(right),
            leftWeight = getWeight(groupId, leftKey),
            rightWeight = getWeight(groupId, rightKey);

        if (leftWeight !== rightWeight) return leftWeight - rightWeight;
        return leftKey.localeCompare(rightKey);
    });
}

function getIncludePreamble(lines) {
    let index = 0;

    while (index < lines.length) {
        const line = lines[index],
            trimmed = line.trim();

        if (trimmed === "#pragma once" || Util.isBlankLine(line) || Util.isCommentOnlyLine(line)) {
            index += 1;
            continue;
        }

        break;
    }

    const startIndex = index;
    if (startIndex >= lines.length) return null;

    const entries = [];

    while (index < lines.length) {
        const line = lines[index];

        if (Util.isBlankLine(line)) {
            index += 1;
            continue;
        }

        const includeInfo = parseIncludeLine(line);
        if (includeInfo) {
            entries.push({
                type: "include",
                includePath: includeInfo.includePath,
                delimiter: includeInfo.delimiter
            });
            index += 1;
            continue;
        }

        if (isUsingNamespaceLine(line, "Backend")) {
            entries.push({
                type: "using-backend",
                line: IncludeRules.backendUsing
            });
            index += 1;
            continue;
        }

        if (isUsingNamespaceLine(line, "GUI")) {
            entries.push({
                type: "using-gui",
                line: IncludeRules.guiUsing
            });
            index += 1;
            continue;
        }

        if (isConditionalStart(line)) {
            const conditionalEntry = parseConditionalBlock(lines, index);
            if (!conditionalEntry) return null;

            entries.push(conditionalEntry);
            index = conditionalEntry.endIndex + 1;
            continue;
        }

        break;
    }

    if (entries.length < 1) return null;

    return {
        startIndex,
        endIndex: index,
        prefixLines: lines.slice(0, startIndex),
        suffixLines: lines.slice(index),
        entries
    };
}

function renderEntry(fileInfo, entry, forceBasename = false) {
    if (entry.type === "include") {
        const normalizedIncludePath = normalizeIncludePath(fileInfo, entry, forceBasename);
        return [buildIncludeLine(entry.delimiter, normalizedIncludePath)];
    }

    if (entry.type === "conditional") {
        return normalizeConditionalLines(fileInfo, entry.lines);
    }

    if (entry.type === "using-backend" || entry.type === "using-gui") {
        return [entry.line];
    }

    return [];
}

function formatIncludePreamble(fileInfo, preamble) {
    const topLevelUsingEntries = preamble.entries.filter(
        entry => entry.type === "using-backend" || entry.type === "using-gui"
    );

    if (fileInfo.isHeader && topLevelUsingEntries.length > 0) {
        return null;
    }

    let entries = [...preamble.entries],
        ownHeaderEntry = null,
        uiEntry = null,
        backendUsingEntry = null,
        guiUsingEntry = null;

    if (!fileInfo.isHeader) {
        const ownHeaderIndex = entries.findIndex(
            entry => entry.type === "include" && isOwnHeaderInclude(fileInfo, entry)
        );
        if (ownHeaderIndex >= 0) ownHeaderEntry = entries.splice(ownHeaderIndex, 1)[0];

        const uiIndex = entries.findIndex(entry => entry.type === "include" && isUIInclude(entry));
        if (uiIndex >= 0) uiEntry = entries.splice(uiIndex, 1)[0];

        const backendUsingIndex = entries.findIndex(entry => entry.type === "using-backend");
        if (backendUsingIndex >= 0) backendUsingEntry = entries.splice(backendUsingIndex, 1)[0];

        const guiUsingIndex = entries.findIndex(entry => entry.type === "using-gui");
        if (guiUsingIndex >= 0) guiUsingEntry = entries.splice(guiUsingIndex, 1)[0];
    } else if (entries.some(entry => entry.type === "using-backend" || entry.type === "using-gui")) {
        return null;
    }

    if (entries.some(entry => entry.type === "using-backend" || entry.type === "using-gui")) {
        return null;
    }

    const groupedEntries = new Map(IncludeRules.groupOrder.map(groupId => [groupId, []]));

    for (const entry of entries) {
        const groupId = entry.type === "conditional" ? getConditionalGroup(entry) : getIncludeGroup(entry);
        groupedEntries.get(groupId).push(entry);
    }

    const groups = [];

    if (ownHeaderEntry || uiEntry) {
        const initialGroup = [];

        if (ownHeaderEntry) initialGroup.push(...renderEntry(fileInfo, ownHeaderEntry, true));
        if (uiEntry) initialGroup.push(...renderEntry(fileInfo, uiEntry, true));
        if (initialGroup.length > 0) groups.push(initialGroup);
    }

    for (const groupId of IncludeRules.groupOrder) {
        const entriesInGroup = sortEntries(groupId, groupedEntries.get(groupId));
        if (entriesInGroup.length < 1) continue;

        const renderedGroup = [];

        for (const entry of entriesInGroup) renderedGroup.push(...renderEntry(fileInfo, entry));
        if (groupId === IncludeGroups.backend && backendUsingEntry) renderedGroup.push(backendUsingEntry.line);
        if (renderedGroup.length > 0) groups.push(renderedGroup);
    }

    if (guiUsingEntry) groups.push([guiUsingEntry.line]);
    if (groups.length < 1) return null;

    const formattedPreambleLines = [];

    for (let i = 0; i < groups.length; i += 1) {
        if (i > 0) formattedPreambleLines.push("");
        formattedPreambleLines.push(...groups[i]);
    }

    const prefixLines = Util.trimEndBlankLines(preamble.prefixLines),
        suffixLines = Util.trimStartBlankLines(preamble.suffixLines),
        output = [...prefixLines];

    if (output.length > 0 && formattedPreambleLines.length > 0) {
        output.push("");
    }

    output.push(...formattedPreambleLines);

    if (suffixLines.length > 0) {
        output.push("");
        output.push(...suffixLines);
    }

    return output;
}

function formatFile(filePath) {
    const oldText = FileUtil.readFile(filePath),
        newline = oldText.includes("\r\n") ? "\r\n" : "\n",
        lines = oldText.split(/\r?\n/);

    while (lines.length > 0 && lines[lines.length - 1] === "") {
        lines.pop();
    }

    const preamble = getIncludePreamble(lines);
    if (!preamble) return false;

    const fileInfo = getFileInfo(filePath),
        formattedLines = formatIncludePreamble(fileInfo, preamble);

    if (!formattedLines) return false;

    const hadTrailingNewline = oldText.endsWith("\n"),
        bodyText = formattedLines.join(newline),
        newText = hadTrailingNewline ? `${bodyText}${newline}` : bodyText;

    return FileUtil.writeIfChanged(filePath, oldText, newText, args.check);
}

async function parseArgs() {
    const parsed = parseNodeArgs({
        args: process.argv.slice(2),
        allowPositionals: true,
        options: {
            "repo-root": {
                type: "string"
            },
            yes: {
                type: "boolean",
                short: "y"
            },
            "source-roots": {
                type: "string",
                multiple: true
            },
            paths: {
                type: "string",
                multiple: true
            },
            check: {
                type: "boolean"
            },
            help: {
                type: "boolean",
                short: "h"
            }
        }
    });

    if (parsed.values.help) {
        console.log(usage);
        process.exit(0);
    }

    let repoRoot = path.resolve(parsed.values["repo-root"] || getRepoRoot(process.argv[1]));

    if (!parsed.values["repo-root"] && !parsed.values.yes) {
        repoRoot = await confirmRepoRoot(repoRoot, usage);
    }

    const sourceRoots = (parsed.values["source-roots"]?.flatMap(FormatterUtil.parseList) ?? defaultSourceRoots).map(
            entry => path.resolve(repoRoot, entry)
        ),
        optionPaths = parsed.values.paths?.flatMap(FormatterUtil.parseList) ?? [];

    return {
        repoRoot,
        sourceRoots,
        includeSearchRoots: defaultIncludeSearchRoots.map(entry => path.resolve(repoRoot, entry)),
        paths: [...optionPaths, ...parsed.positionals].map(entry => path.resolve(repoRoot, entry)),
        excludeFile: path.resolve(repoRoot, ".format-exclude"),
        check: parsed.values.check ?? false
    };
}

async function main() {
    args = await parseArgs();

    const targetFiles = getTargetFiles(args, usage);

    if (targetFiles.length < 1) {
        console.error("ERROR: No target files found.", "\n");
        console.log(usage);

        process.exit(1);
    }

    let changedCount = 0;

    for (const filePath of targetFiles) {
        if (!formatFile(filePath)) {
            continue;
        }

        changedCount += 1;

        if (args.check) {
            console.log("Would format:", filePath);
        } else {
            console.log("Formatted:", filePath);
        }
    }

    process.exit(args.check && changedCount > 0 ? 1 : 0);
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});
