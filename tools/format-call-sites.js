const path = require("path");
const { parseArgs: parseNodeArgs } = require("util");
const {
    FileUtil,
    FormatterUtil,
    confirmRepoRoot,
    getRepoRoot,
    getTargetFiles
} = require("./util/formatter-util");

const defaultSourceRoots = ["common", "frontend", "mandelbrot-cli", "mandelbrot", "mandelbrot-gui"],
    defaultMaxWidth = 80;

let args = null;

const usage = `Usage: node ./tools/format-call-sites.js [paths...] [options]

Options:
  --repo-root <path>        Repository root
  --source-roots <items>    Comma-separated source roots
  --paths <items>           Comma-separated files or directories
  --width <columns>         Maximum line width (default: 80)
  -y                        Trust the deduced repo root
  --check                   Report files that would change and exit 1
  --help                    Show this help`;

const Util = Object.freeze({
    countParenDepth: str => {
        let depth = 0;

        for (const char of str) {
            if (char === "(") depth += 1;
            else if (char === ")") depth -= 1;
        }

        return depth;
    },

    splitTopLevelArgs: str => {
        const groups = [];

        let current = "",
            parenDepth = 0,
            angleDepth = 0,
            bracketDepth = 0,
            braceDepth = 0,
            stringQuote = null,
            escaping = false;

        for (let i = 0; i < str.length; i += 1) {
            const char = str[i];

            if (stringQuote) {
                current += char;

                if (escaping) {
                    escaping = false;
                    continue;
                }

                if (char === "\\") {
                    escaping = true;
                } else if (char === stringQuote) {
                    stringQuote = null;
                }

                continue;
            }

            if (char === '"' || char === "'") {
                stringQuote = char;
                current += char;
                continue;
            }

            if (char === "(") parenDepth += 1;
            else if (char === ")" && parenDepth > 0) parenDepth -= 1;
            else if (char === "<") angleDepth += 1;
            else if (char === ">" && angleDepth > 0) angleDepth -= 1;
            else if (char === "[") bracketDepth += 1;
            else if (char === "]" && bracketDepth > 0) bracketDepth -= 1;
            else if (char === "{") braceDepth += 1;
            else if (char === "}" && braceDepth > 0) braceDepth -= 1;

            if (char === "," && parenDepth === 0 && angleDepth === 0 && bracketDepth === 0 && braceDepth === 0) {
                groups.push(current.trim());
                current = "";
                continue;
            }

            current += char;
        }

        const tail = current.trim();
        if (tail) groups.push(tail);

        return groups;
    },

    packGroups: (groups, width) => {
        const lines = [];

        let current = "";

        for (const group of groups) {
            const trimmed = group.trim(),
                candidate = current ? `${current}, ${trimmed}` : trimmed;

            if (!current || candidate.length <= width) {
                current = candidate;
                continue;
            }

            lines.push(current);
            current = trimmed;
        }

        if (current) lines.push(current);
        return lines;
    },

    isQualifiedConstructorLikeName: trimmed => {
        const parts = trimmed.split("::");

        if (parts.length < 2) return false;

        const leaf = parts[parts.length - 1],
            owner = parts[parts.length - 2];

        if (leaf.startsWith("operator")) return true;

        const normalizedLeaf = leaf.startsWith("~") ? leaf.slice(1) : leaf;
        return normalizedLeaf === owner;
    },

    testSignaturePrefix: prefix => {
        const trimmed = prefix.trim(),
            normalizedControlFlowPrefix = trimmed.replace(/^(?:\}\s*)+/, "").trimStart();

        if (!trimmed) return false;
        if (/^(if|for|while|switch|catch|return|else|do|sizeof|static_assert)\b/.test(normalizedControlFlowPrefix)) {
            return false;
        }
        if (trimmed.startsWith("#")) return false;
        if (trimmed.includes("=")) return false;
        if (trimmed.includes("->") || trimmed.includes(".")) return false;
        if (/^[A-Z0-9_]+$/.test(trimmed)) return false;
        if (trimmed.startsWith("~") || trimmed.startsWith("operator")) return true;
        if (trimmed.includes(" ")) return true;
        if (/^(?:~?\w+|operator\S+)$/.test(trimmed)) return true;

        return Util.isQualifiedConstructorLikeName(trimmed);
    },

    testSignatureSuffix: suffix => {
        return /^(?:(?:const|noexcept|override|final)\s*)*(?:=\s*(?:0|delete|default)\s*)?[;{]$/.test(suffix);
    },

    isBraceSuffix: suffix => {
        return suffix.endsWith("{");
    },

    getOriginalArgumentGroups: argumentText => {
        const argumentGroups = [];

        for (const rawLine of argumentText.split("\n")) {
            const line = rawLine.trim().replace(/^,\s*/, "").replace(/,\s*$/, "");
            if (!line) continue;

            const lineGroups = Util.splitTopLevelArgs(line);
            if (lineGroups.length < 1) return null;

            argumentGroups.push(lineGroups);
        }

        return argumentGroups.length > 0 ? argumentGroups : null;
    },

    reflowPreservedArgumentLines: (originalArgumentGroups, width) => {
        const argumentLines = [];

        for (const groups of originalArgumentGroups) {
            const line = groups.join(", ");

            if (line.length <= width) {
                argumentLines.push(line);
                continue;
            }

            argumentLines.push(...Util.packGroups(groups, width));
        }

        return argumentLines;
    },

    containsCommentTokens: text => {
        return /\/\/|\/\*/.test(text);
    },

    allWhitespaceAfterColumn: (line, column) => {
        return line.slice(column + 1).trim() === "";
    },

    exceedsWidth: line => {
        return line.length > args.maxWidth;
    },

    isInsideFunctionBody: scopeStack => {
        return scopeStack.includes("function");
    }
});

function findCandidateOpenColumns(line) {
    const columns = [];

    let stringQuote = null,
        escaping = false,
        inBlockComment = false;

    for (let i = 0; i < line.length; i += 1) {
        const char = line[i],
            next = line[i + 1];

        if (inBlockComment) {
            if (char === "*" && next === "/") {
                inBlockComment = false;
                i += 1;
            }
            continue;
        }

        if (stringQuote) {
            if (escaping) {
                escaping = false;
                continue;
            }

            if (char === "\\") {
                escaping = true;
                continue;
            }

            if (char === stringQuote) {
                stringQuote = null;
            }

            continue;
        }

        if (char === "/" && next === "/") {
            break;
        }

        if (char === "/" && next === "*") {
            inBlockComment = true;
            i += 1;
            continue;
        }

        if (char === '"' || char === "'") {
            stringQuote = char;
            continue;
        }

        if (char === "(" && Util.allWhitespaceAfterColumn(line, i)) {
            columns.push(i);
        }
    }

    return columns.reverse();
}

function findMatchingParen(lines, startLineIndex, openColumn) {
    let depth = 0,
        stringQuote = null,
        escaping = false,
        inBlockComment = false;

    for (let lineIndex = startLineIndex; lineIndex < lines.length; lineIndex += 1) {
        const line = lines[lineIndex];

        for (let column = lineIndex === startLineIndex ? openColumn : 0; column < line.length; column += 1) {
            const char = line[column],
                next = line[column + 1];

            if (inBlockComment) {
                if (char === "*" && next === "/") {
                    inBlockComment = false;
                    column += 1;
                }
                continue;
            }

            if (stringQuote) {
                if (escaping) {
                    escaping = false;
                    continue;
                }

                if (char === "\\") {
                    escaping = true;
                    continue;
                }

                if (char === stringQuote) {
                    stringQuote = null;
                }

                continue;
            }

            if (char === "/" && next === "/") {
                break;
            }

            if (char === "/" && next === "*") {
                inBlockComment = true;
                column += 1;
                continue;
            }

            if (char === '"' || char === "'") {
                stringQuote = char;
                continue;
            }

            if (char === "(") {
                depth += 1;
                continue;
            }

            if (char === ")") {
                depth -= 1;

                if (depth === 0) {
                    return {
                        lineIndex,
                        column
                    };
                }
            }
        }
    }

    return null;
}

function getArgumentText(blockLines, openColumn, closeColumn) {
    const lines = [...blockLines];

    lines[0] = lines[0].slice(openColumn + 1);
    lines[lines.length - 1] = lines[lines.length - 1].slice(0, closeColumn);

    return lines.join("\n").trim();
}

function getArgumentLines(blockLines, openColumn, closeColumn) {
    const lines = [...blockLines];

    lines[0] = lines[0].slice(openColumn + 1);
    lines[lines.length - 1] = lines[lines.length - 1].slice(0, closeColumn);

    while (lines.length > 0 && lines[0].trim() === "") {
        lines.shift();
    }

    while (lines.length > 0 && lines[lines.length - 1].trim() === "") {
        lines.pop();
    }

    return lines;
}

function getSignatureInfoFromBlock(blockLines) {
    const blockText = blockLines.join("\n"),
        openIndex = blockText.indexOf("(");

    if (openIndex < 0) return null;

    let depth = 0,
        closeIndex = -1;

    for (let i = openIndex; i < blockText.length; i += 1) {
        const char = blockText[i];

        if (char === "(") {
            depth += 1;
        } else if (char === ")") {
            depth -= 1;

            if (depth === 0) {
                closeIndex = i;
                break;
            }
        }
    }

    if (closeIndex < 0) return null;

    const prefix = blockText.slice(0, openIndex),
        suffix = blockText.slice(closeIndex + 1).trim();

    if (!Util.testSignaturePrefix(prefix)) return null;
    if (!Util.testSignatureSuffix(suffix)) return null;

    return {
        suffix
    };
}

function updateScopeStack(scopeStack, text, openedScopeType = null) {
    let usedOpenedScopeType = false;

    for (const char of text) {
        if (char === "{") {
            scopeStack.push(!usedOpenedScopeType && openedScopeType ? openedScopeType : "other");
            usedOpenedScopeType = true;
        } else if (char === "}" && scopeStack.length > 0) {
            scopeStack.pop();
        }
    }
}

function getInsideFunctionBodyLines(lines) {
    const insideFunctionBodyByLine = new Array(lines.length).fill(false);

    let scopeStack = [],
        index = 0;

    while (index < lines.length) {
        insideFunctionBodyByLine[index] = Util.isInsideFunctionBody(scopeStack);

        const line = lines[index],
            openIndex = line.indexOf("(");

        if (openIndex < 0) {
            updateScopeStack(scopeStack, line);
            index += 1;
            continue;
        }

        const block = [line];

        let depth = Util.countParenDepth(line),
            endIndex = index;

        while (depth > 0 && endIndex + 1 < lines.length) {
            endIndex += 1;

            const nextLine = lines[endIndex];
            block.push(nextLine);
            depth += Util.countParenDepth(nextLine);
        }

        const signatureInfo = getSignatureInfoFromBlock(block),
            opensFunctionScope = signatureInfo && Util.isBraceSuffix(signatureInfo.suffix);

        for (let i = index + 1; i <= endIndex; i += 1) {
            insideFunctionBodyByLine[i] = Util.isInsideFunctionBody(scopeStack);
        }

        updateScopeStack(scopeStack, block.join("\n"), opensFunctionScope ? "function" : null);
        index = endIndex + 1;
    }

    return insideFunctionBodyByLine;
}

function formatVerticalCall(prefix, argumentLines, indent, suffix) {
    const argumentIndent = `${indent}    `,
        formatted = [`${indent}${prefix}(`];

    for (let i = 0; i < argumentLines.length; i += 1) {
        const line = argumentLines[i],
            suffixText = i < argumentLines.length - 1 ? "," : "";

        formatted.push(`${argumentIndent}${line}${suffixText}`);
    }

    formatted.push(`${indent})${suffix}`);
    return formatted;
}

function buildVerticalArgumentLines(groups, originalArgumentGroups, width) {
    if (originalArgumentGroups) {
        return Util.reflowPreservedArgumentLines(originalArgumentGroups, width);
    }

    return Util.packGroups(groups, width);
}

function formatHangingCall(prefix, groups, indent, suffix) {
    const hangingOpen = `${indent}${prefix}(`,
        argumentIndent = `${indent}    `,
        firstLineGroups = [];

    for (let i = 0; i < groups.length; i += 1) {
        const candidateGroups = [...firstLineGroups, groups[i]],
            candidateText = candidateGroups.join(", "),
            hasRemainingGroups = i < groups.length - 1,
            closingLength = hasRemainingGroups ? 1 : suffix.length + 1;

        if (hangingOpen.length + candidateText.length + closingLength <= args.maxWidth) {
            firstLineGroups.push(groups[i]);
            continue;
        }

        break;
    }

    if (firstLineGroups.length < 1) {
        return null;
    }

    const remainingGroups = groups.slice(firstLineGroups.length);

    if (remainingGroups.length < 1) {
        const singleLine = `${hangingOpen}${firstLineGroups.join(", ")})${suffix}`;
        return singleLine.length <= args.maxWidth ? [singleLine] : null;
    }

    const preservedWidth = args.maxWidth - argumentIndent.length - Math.max(1, suffix.length + 1),
        remainingLines = Util.packGroups(remainingGroups, preservedWidth),
        formatted = [`${hangingOpen}${firstLineGroups.join(", ")},`];

    for (let i = 0; i < remainingLines.length; i += 1) {
        const line = remainingLines[i];

        if (i === remainingLines.length - 1) {
            formatted.push(`${argumentIndent}${line})${suffix}`);
        } else {
            formatted.push(`${argumentIndent}${line},`);
        }
    }

    if (formatted.some(Util.exceedsWidth)) {
        return null;
    }

    return formatted;
}

function formatConservativeCall(blockLines, openColumn, closeColumn, indent, prefixTrimmed, suffix) {
    const argumentLines = getArgumentLines(blockLines, openColumn, closeColumn);
    if (argumentLines.length < 1) {
        return [`${indent}${prefixTrimmed}()${suffix}`];
    }

    const firstLine = argumentLines[0].trim(),
        trailingLines = argumentLines.slice(1),
        hangingOpen = `${indent}${prefixTrimmed}(${firstLine}`;

    if (trailingLines.length < 1) {
        const singleLine = `${hangingOpen})${suffix}`;
        if (singleLine.length <= args.maxWidth) {
            return [singleLine];
        }
    } else if (hangingOpen.length <= args.maxWidth) {
        const formatted = [hangingOpen];

        for (let i = 0; i < trailingLines.length; i += 1) {
            const line = trailingLines[i];

            if (i === trailingLines.length - 1) {
                formatted.push(`${line})${suffix}`);
            } else {
                formatted.push(line);
            }
        }

        return formatted;
    }

    return [
        `${indent}${prefixTrimmed}(`,
        ...argumentLines,
        `${indent})${suffix}`
    ];
}

function formatCallBlock(blockLines, openColumn, closeColumn, insideFunctionBody) {
    const indent = blockLines[0].match(/^\s*/)?.[0] ?? "",
        prefixTrimmed = blockLines[0].slice(indent.length, openColumn).trimEnd(),
        suffix = blockLines[blockLines.length - 1].slice(closeColumn + 1).trim(),
        argumentText = getArgumentText(blockLines, openColumn, closeColumn);

    if (!prefixTrimmed) return null;
    if (!insideFunctionBody && Util.testSignaturePrefix(prefixTrimmed) && Util.testSignatureSuffix(suffix)) {
        return null;
    }
    if (Util.containsCommentTokens(argumentText)) return null;

    if (!argumentText) {
        return [`${indent}${prefixTrimmed}()${suffix}`];
    }

    const groups = Util.splitTopLevelArgs(argumentText);
    if (groups.length < 1) return null;

    const originalArgumentGroups = Util.getOriginalArgumentGroups(argumentText),
        flattenedOriginalGroupCount = originalArgumentGroups
            ? originalArgumentGroups.reduce((sum, entry) => sum + entry.length, 0)
            : groups.length;

    if (flattenedOriginalGroupCount !== groups.length) {
        return formatConservativeCall(blockLines, openColumn, closeColumn, indent, prefixTrimmed, suffix);
    }

    const singleLine = `${prefixTrimmed}(${groups.join(", ")})${suffix}`;
    if (indent.length + singleLine.length <= args.maxWidth) {
        return [`${indent}${singleLine}`];
    }

    const hanging = formatHangingCall(prefixTrimmed, groups, indent, suffix);
    if (hanging) return hanging;

    const argumentIndent = `${indent}    `,
        verticalArgumentLines = buildVerticalArgumentLines(
            groups,
            originalArgumentGroups,
            args.maxWidth - argumentIndent.length
        );

    return formatVerticalCall(prefixTrimmed, verticalArgumentLines, indent, suffix);
}

function formatFile(filePath) {
    const oldText = FileUtil.readFile(filePath),
        newline = oldText.includes("\r\n") ? "\r\n" : "\n";

    const hadTrailingNewline = oldText.endsWith("\n"),
        lines = oldText.split(/\r?\n/);

    if (hadTrailingNewline) {
        lines.pop();
    }

    const insideFunctionBodyByLine = getInsideFunctionBodyLines(lines);

    let changed = false;

    for (let lineIndex = lines.length - 1; lineIndex >= 0; lineIndex -= 1) {
        const openColumns = findCandidateOpenColumns(lines[lineIndex]);

        let replaced = false;

        for (const openColumn of openColumns) {
            const closeInfo = findMatchingParen(lines, lineIndex, openColumn);
            if (!closeInfo) continue;

            const blockLines = lines.slice(lineIndex, closeInfo.lineIndex + 1),
                formattedBlock = formatCallBlock(
                    blockLines,
                    openColumn,
                    closeInfo.column,
                    insideFunctionBodyByLine[lineIndex]
                );

            if (!formattedBlock) continue;

            const oldBlockText = blockLines.join("\n"),
                newBlockText = formattedBlock.join("\n");

            if (oldBlockText === newBlockText) {
                continue;
            }

            lines.splice(lineIndex, closeInfo.lineIndex - lineIndex + 1, ...formattedBlock);
            changed = true;
            replaced = true;
            break;
        }

        if (replaced) {
            continue;
        }
    }

    const bodyText = lines.join(newline),
        newText = hadTrailingNewline ? `${bodyText}${newline}` : bodyText;

    return FileUtil.writeIfChanged(filePath, oldText, newText, args.check) || changed;
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
            width: {
                type: "string"
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

    const maxWidth = parsed.values.width ? Number.parseInt(parsed.values.width, 10) : defaultMaxWidth;

    if (!Number.isInteger(maxWidth) || maxWidth < 20) {
        console.error("ERROR: --width must be an integer >= 20.", "\n");
        console.log(usage);
        process.exit(1);
    }

    return {
        repoRoot,
        sourceRoots,
        paths: [...optionPaths, ...parsed.positionals].map(entry => path.resolve(repoRoot, entry)),
        excludeFile: path.resolve(repoRoot, ".format-exclude"),
        maxWidth,
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
