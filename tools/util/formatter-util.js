const fs = require("fs");
const path = require("path");
const readline = require("readline");

const ExtensionGroups = {
    headerExtensions: new Set([".h", ".hpp"]),
    sourceExtensions: new Set([".c", ".cpp", "_impl.h", "_impl.hpp"])
};

const targetExtensions = new Set([...ExtensionGroups.headerExtensions, ...ExtensionGroups.sourceExtensions]);

const FormatterUtil = Object.freeze({
    parseList: str => {
        return str
            .split(",")
            .map(x => x.trim())
            .filter(Boolean);
    },

    normalizeSlashes: str => {
        return str.replaceAll("\\", "/");
    },

    getFileSuffixes: filePath => {
        const normalizedPath = FormatterUtil.normalizeSlashes(filePath).toLowerCase(),
            leafName = path.posix.basename(normalizedPath),
            suffixes = new Set();

        for (let i = 0; i < leafName.length; i += 1) {
            const char = leafName[i];
            if (char !== "." && char !== "_") continue;

            suffixes.add(leafName.slice(i));
        }

        return suffixes;
    },

    hasAnySuffix: (suffixes, extensions) => {
        for (const suffix of suffixes) {
            if (extensions.has(suffix)) return true;
        }

        return false;
    }
});

const FileUtil = Object.freeze({
    readFile: filePath => {
        try {
            return fs.readFileSync(filePath, "utf8");
        } catch (err) {
            if (err.code === "ENOENT") {
                console.error(`ERROR: File not found at path: "${filePath}".`);
            } else {
                console.error("ERROR: Occured while reading the file:");
                console.error(err);
            }

            throw err;
        }
    },

    writeFile: (filePath, text) => {
        try {
            fs.writeFileSync(filePath, text, "utf8");
        } catch (err) {
            console.error("ERROR: Occured while writing the file:");
            console.error(err);

            throw err;
        }

        return filePath;
    },

    writeIfChanged: (filePath, oldText, newText, check) => {
        const changed = newText !== oldText;

        if (changed && !check) {
            FileUtil.writeFile(filePath, newText);
        }

        return changed;
    }
});

function getRepoRoot(scriptPath) {
    return path.resolve(path.dirname(scriptPath), "..");
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

async function confirmRepoRoot(repoRoot, usage) {
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

function isHeaderFile(filePath) {
    const suffixes = FormatterUtil.getFileSuffixes(filePath),
        isHeader = FormatterUtil.hasAnySuffix(suffixes, ExtensionGroups.headerExtensions),
        isSource = FormatterUtil.hasAnySuffix(suffixes, ExtensionGroups.sourceExtensions);

    return isHeader && !isSource;
}

function isTargetExtension(filePath) {
    return FormatterUtil.hasAnySuffix(FormatterUtil.getFileSuffixes(filePath), targetExtensions);
}

function listFiles(rootPath) {
    const output = [];

    for (const entry of fs.readdirSync(rootPath, { withFileTypes: true })) {
        const entryPath = path.resolve(rootPath, entry.name);

        if (entry.isDirectory()) {
            output.push(...listFiles(entryPath));
        } else if (entry.isFile()) {
            output.push(entryPath);
        }
    }

    return output;
}

function loadExcludePaths(excludeFile, sourceRoots) {
    if (!fs.existsSync(excludeFile)) return new Set();

    const lines = FileUtil
        .readFile(excludeFile)
        .split(/\r?\n/)
        .map(line => FormatterUtil.normalizeSlashes(line.trim()))
        .filter(line => line && !line.startsWith("#"));

    const excludePaths = new Set();

    for (const sourceRoot of sourceRoots) {
        for (const line of lines) {
            excludePaths.add(path.resolve(sourceRoot, line));
        }
    }

    return excludePaths;
}

function isExcludedFile(filePath, excludePaths) {
    return excludePaths.has(path.resolve(filePath));
}

function getTargetFiles(args, usage) {
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

module.exports = {
    ExtensionGroups,
    FileUtil,
    FormatterUtil,
    confirmRepoRoot,
    getTargetFiles,
    getRepoRoot,
    isHeaderFile,
    targetExtensions
};
