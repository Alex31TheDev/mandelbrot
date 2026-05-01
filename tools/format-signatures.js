const fs = require("fs");
const path = require("path");
const readline = require("readline");
const { parseArgs: parseNodeArgs } = require("util");

const defaultSourceRoots = ["common", "frontend", "mandelbrot-cli", "mandelbrot", "mandelbrot-gui"],
    targetExtensions = [".h", ".cpp"];

const usage = `Usage: node ./tools/format-signatures.js [paths...] [options]

Options:
  --repo-root <path>        Repository root
  --source-roots <items>    Comma-separated source roots
  --paths <items>           Comma-separated files or directories
  -y                        Trust the deduced repo root
  --check                   Report files that would change and exit 1
  --help                    Show this help`;

const Util = {
    parseList: str => {
        return str
            .split(",")
            .map(x => x.trim())
            .filter(Boolean);
    },

    normalizeSlashes: str => {
        return str.replaceAll("\\", "/");
    },

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
            braceDepth = 0;

        for (let i = 0; i < str.length; i += 1) {
            const char = str[i];

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

    packArgumentLines: (groups, width) => {
        let lines = [],
            current = "";

        for (const group of groups) {
            const trimmed = group.trim(),
                candidate = current ? `${current}, ${trimmed}` : trimmed;

            if (candidate.length <= width) {
                current = candidate;
                continue;
            }

            if (current) lines.push(current);
            current = trimmed;
        }

        if (current) lines.push(current);
        return lines;
    },

    testSignaturePrefix: prefix => {
        const trimmed = prefix.trim();

        if (!trimmed) return false;
        else if (/^(if|for|while|switch|catch|return|else|do|sizeof)\b/.test(trimmed)) return false;
        else if (trimmed.startsWith("#")) return false;
        else if (trimmed.includes("=")) return false;
        else if (trimmed.includes("->") || trimmed.includes(".")) return false;
        else if (/^[A-Z0-9_]+$/.test(trimmed)) return false;
        else
            return (
                trimmed.includes(" ") ||
                trimmed.includes("::") ||
                trimmed.startsWith("~") ||
                trimmed.startsWith("operator")
            );
    },

    testSignatureSuffix: suffix => {
        return /^(?:\{|;|const\s*;|const\s*\{|noexcept\s*;|noexcept\s*\{|override\s*;|override\s*\{|final\s*;|final\s*\{|=\s*0\s*;|=\s*delete\s*;|=\s*default\s*;|const\s+override\s*;|const\s+override\s*\{)/.test(
            suffix
        );
    },

    getSuffixText: suffix => {
        if (!suffix) return "";
        return suffix === ";" ? ";" : ` ${suffix}`;
    }
};

function getRepoRoot() {
    return path.resolve(path.dirname(process.argv[1]), "..");
}

function ask(question) {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    return new Promise(resolve => {
        rl.question(question, answer => {
            rl.close();
            resolve(answer);
        });
    });
}

async function confirmRepoRoot(repoRoot) {
    if (!process.stdin?.isTTY || !process.stdout?.isTTY) {
        console.error("ERROR: Refusing to guess the repo root in non-interactive mode.");
        console.error("Pass --repo-root explicitly, or use -y to trust the deduced root.", "\n");
        console.log(usage);

        process.exit(1);
    }

    const answer = await ask(`Use deduced repo root "${repoRoot}"? [y/N] `),
        normalized = answer?.trim().toLowerCase() ?? "";

    if (normalized === "y" || normalized === "yes") {
        return repoRoot;
    }

    console.error("ERROR: Repo root not confirmed.");
    console.error("Pass --repo-root explicitly, or use -y to trust the deduced root.", "\n");
    console.log(usage);

    process.exit(1);
}

function loadExcludePaths(excludeFile, sourceRoots) {
    if (!fs.existsSync(excludeFile)) return new Set();

    const lines = fs
        .readFileSync(excludeFile, "utf8")
        .split(/\r?\n/)
        .map(line => Util.normalizeSlashes(line.trim()))
        .filter(line => line && !line.startsWith("#"));

    const excludePaths = new Set();

    for (const sourceRoot of sourceRoots) {
        for (const line of lines) {
            excludePaths.add(path.resolve(sourceRoot, line));
        }
    }

    return excludePaths;
}

function isTargetExtension(filePath) {
    return targetExtensions.includes(path.extname(filePath).toLowerCase());
}

function isExcludedFile(filePath, excludePaths) {
    return excludePaths.has(path.resolve(filePath));
}

function listFiles(directoryPath, files = []) {
    const entries = fs.readdirSync(directoryPath, { withFileTypes: true });

    for (const entry of entries) {
        const entryPath = path.join(directoryPath, entry.name);

        if (entry.isDirectory()) {
            listFiles(entryPath, files);
            continue;
        }

        if (entry.isFile()) files.push(entryPath);
    }

    return files;
}

function formatVerticalSignature(prefix, groups, indent, suffixText) {
    const argumentIndent = `${indent}    `,
        argumentLines = Util.packArgumentLines(groups, 80 - argumentIndent.length);

    const formatted = [`${indent}${prefix}(`];

    for (let i = 0; i < argumentLines.length; i += 1) {
        const line = argumentLines[i];

        if (i < argumentLines.length - 1) {
            formatted.push(`${argumentIndent}${line},`);
        } else {
            formatted.push(`${argumentIndent}${line}`);
        }
    }

    formatted.push(`${indent})${suffixText}`);
    return formatted;
}

function formatHeaderMultiline(prefix, groups, indent, suffixText) {
    const argumentIndent = `${indent}    `,
        hangingOpen = `${indent}${prefix}(`;

    const firstLineGroups = [];

    for (const group of groups) {
        const candidateGroups = [...firstLineGroups, group],
            candidateLine = candidateGroups.join(", ");

        if (hangingOpen.length + candidateLine.length + 1 <= 80) {
            firstLineGroups.push(group);
            continue;
        }

        break;
    }

    if (firstLineGroups.length < 1) {
        return formatVerticalSignature(prefix, groups, indent, suffixText);
    }

    const remainingGroups = groups.slice(firstLineGroups.length);

    if (remainingGroups.length < 1) {
        return [`${hangingOpen}${firstLineGroups.join(", ")})${suffixText}`];
    }

    const remainingLines = Util.packArgumentLines(remainingGroups, 80 - argumentIndent.length),
        formatted = [`${hangingOpen}${firstLineGroups.join(", ")},`];

    for (let i = 0; i < remainingLines.length; i += 1) {
        const line = remainingLines[i];

        if (i === remainingLines.length - 1) {
            formatted.push(`${argumentIndent}${line})${suffixText}`);
        } else {
            formatted.push(`${argumentIndent}${line},`);
        }
    }

    return formatted;
}

function formatSignatureBlock(blockLines, isHeader) {
    const blockText = blockLines.join("\n"),
        openIndex = blockText.indexOf("(");

    if (openIndex < 0) return blockLines;

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

    if (closeIndex < 0) return blockLines;

    const prefix = blockText.slice(0, openIndex);
    if (!Util.testSignaturePrefix(prefix)) return blockLines;

    const suffix = blockText.slice(closeIndex + 1).trim();
    if (!Util.testSignatureSuffix(suffix)) return blockLines;

    const indent = blockLines[0].match(/^\s*/)?.[0] ?? "";

    const prefixTrimmed = prefix.slice(indent.length).trimEnd(),
        argumentText = blockText.slice(openIndex + 1, closeIndex).trim();

    if (!argumentText) return blockLines;

    const groups = Util.splitTopLevelArgs(argumentText),
        suffixText = Util.getSuffixText(suffix);

    const singleLine = `${prefixTrimmed}(${groups.join(", ")})${suffixText}`;

    if (indent.length + singleLine.length <= 80) {
        return [`${indent}${singleLine}`];
    }

    return (isHeader ? formatHeaderMultiline : formatVerticalSignature)(prefixTrimmed, groups, indent, suffixText);
}

function formatFile(filePath, check) {
    const oldText = fs.readFileSync(filePath, "utf8"),
        newline = oldText.includes("\r\n") ? "\r\n" : "\n",
        lines = oldText.split(/\r?\n/);

    const extension = path.extname(filePath).toLowerCase(),
        isHeader = extension === ".h";

    if (lines.length > 0 && lines[lines.length - 1] === "") {
        lines.pop();
    }

    let output = [],
        index = 0;

    while (index < lines.length) {
        const line = lines[index],
            openIndex = line.indexOf("(");

        if (openIndex < 0) {
            output.push(line);
            index += 1;
            continue;
        }

        const prefix = line.slice(0, openIndex);

        if (!Util.testSignaturePrefix(prefix)) {
            output.push(line);
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

        output.push(...formatSignatureBlock(block, isHeader));
        index = endIndex + 1;
    }

    const newText = output.join(newline),
        changed = newText !== oldText;

    if (changed && !check) {
        fs.writeFileSync(filePath, newText, "utf8");
    }

    return changed;
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

    let repoRoot = path.resolve(parsed.values["repo-root"] || getRepoRoot());

    if (!parsed.values["repo-root"] && !parsed.values.yes) {
        repoRoot = await confirmRepoRoot(repoRoot);
    }

    const sourceRoots = (parsed.values["source-roots"]?.flatMap(Util.parseList) ?? defaultSourceRoots).map(entry =>
        path.resolve(repoRoot, entry)
    );

    const optionPaths = parsed.values.paths?.flatMap(Util.parseList) ?? [];

    return {
        repoRoot,
        sourceRoots,
        paths: [...optionPaths, ...parsed.positionals].map(entry => path.resolve(repoRoot, entry)),
        excludeFile: path.resolve(repoRoot, ".format-exclude"),
        check: parsed.values.check ?? false
    };
}

function getTargetFiles(args) {
    const sourceRootPaths = args.sourceRoots.filter(entry => fs.existsSync(entry)),
        scanPaths = args.paths.length > 0 ? args.paths : sourceRootPaths,
        excludePaths = loadExcludePaths(args.excludeFile, sourceRootPaths);

    const files = new Set();

    for (const entryPath of scanPaths) {
        if (!fs.existsSync(entryPath)) {
            console.error("ERROR: Path not found:", entryPath, "\n");
            console.log(usage);

            process.exit(1);
        }

        const resolvedPath = path.resolve(entryPath),
            stats = fs.statSync(resolvedPath);

        if (stats.isDirectory()) {
            const discoveredFiles = listFiles(resolvedPath);

            for (const filePath of discoveredFiles) {
                if (!isTargetExtension(filePath) || isExcludedFile(filePath, excludePaths)) {
                    continue;
                }

                files.add(path.resolve(filePath));
            }

            continue;
        }

        if (!stats.isFile() || !isTargetExtension(resolvedPath) || isExcludedFile(resolvedPath, excludePaths)) {
            continue;
        }

        files.add(resolvedPath);
    }

    return [...files].sort((a, b) => a.localeCompare(b));
}

async function main() {
    const args = await parseArgs(),
        targetFiles = getTargetFiles(args);

    if (targetFiles.length < 1) {
        console.error("ERROR: No target files found.", "\n");
        console.log(usage);

        process.exit(1);
    }

    let changedCount = 0;

    for (const filePath of targetFiles) {
        if (!formatFile(filePath, args.check)) {
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
