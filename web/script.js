function downloadJson(jsonData, fileName) {
    const jsonString = JSON.stringify(jsonData, null, 2);
    const blob = new Blob([jsonString], { type: "application/json" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = fileName;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

function downloadText(text, fileName) {
    const blob = new Blob([text], { type: "text/plain" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = fileName;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

function toText(arr) {
    const recur = ({ Item, Children }) => Item?.Name +
        (Children?.length ? "\n" + Children.map(recur).map((text, i, { length }) =>
            i < length - 1 ? "├──" + text.replace(/\n/g, "\n│  ")
                : "└──" + text.replace(/\n/g, "\n   ")
        ).join("\n") : "")
    return recur(arr)
}


function get_title() {
    return "Oberon 0 Compiler"
}

function setup_editor(monaco, editor_el) {
    monaco.languages.register({ id: 'oberon0' });

    // Define syntax highlighting
    monaco.languages.setMonarchTokensProvider('oberon0', {
        keywords: ['MODULE', 'BEGIN', 'END', 'VAR', 'PROCEDURE', 'RETURN', 'IF', 'THEN', 'ELSE', 'WHILE', 'DO'],
        tokenizer: {
            root: [
                [/\b(?:MODULE|BEGIN|END|VAR|PROCEDURE|RETURN|IF|THEN|ELSE|WHILE|DO)\b/, 'keyword'],
                [/[a-zA-Z_][\w\d]*/, 'identifier'],
                [/\d+/, 'number'],
                [/[+\-*/=]/, 'operator'],
                [/".*?"/, 'string'],
                [/\(\*[\s\S]*?\*\)/, 'comment'],
            ]
        }
    });

    // Add basic auto-completion for Oberon-0
    monaco.languages.registerCompletionItemProvider('oberon0', {
        provideCompletionItems: function () {
            return {
                suggestions: [
                    { label: 'MODULE', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'MODULE ' },
                    { label: 'BEGIN', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'BEGIN ' },
                    { label: 'END', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'END ' },
                    { label: 'VAR', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'VAR ' },
                    { label: 'PROCEDURE', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'PROCEDURE ' },
                    { label: 'RETURN', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'RETURN ' },
                    { label: 'IF', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'IF ' },
                    { label: 'THEN', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'THEN ' },
                    { label: 'ELSE', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'ELSE ' },
                    { label: 'WHILE', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'WHILE ' },
                    { label: 'DO', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'DO ' }
                ]
            };
        }
    });
    const editor = monaco.editor.create(editor_el, {
        value: getCode(),
        language: 'oberon0',
        theme: 'vs-dark'
    });

    return editor;
}

function run_code(editor, output_el) {
    output_el.innerHTML += "&#9203;\n"
    Module().then(instance => {
        const parse = instance.cwrap('parse', 'number', ['string']);
        let ptr = parse(editor.getValue());
        let data = instance.UTF8ToString(ptr);
        output_el.innerText += JSON.stringify(JSON.parse(data), null, 2);
    });
}

function setup_cmd(editor, output_el) {
    const commands = {
        help: () => "Available commands: help, clear, tokens, ast, check, ir, run",
        tokens: (args) => {
            Module().then(instance => {
                const scanner = instance.cwrap('scanner', 'number', ['string']);
                let ptr = scanner(editor.getValue());
                let data = instance.UTF8ToString(ptr);
                data = JSON.parse(data);

                if (args.length === 0) {
                    data["tokens"].forEach(e => output_el.innerText += e["type"] + "\n")
                } else if (args.length === 2 && args[0] == ">") {
                    output_el.innerText += downloadJson(data, args[1]);
                } else {
                    output_el.innerText += "ERROR: unrecognized arguments for `tokens`"
                }
            });
            return "&#9203;";
        },
        ast: (args) => {
            Module().then(instance => {
                const parse = instance.cwrap('parse', 'number', ['string']);
                let ptr = parse(editor.getValue());
                let data = instance.UTF8ToString(ptr);
                data = JSON.parse(data);

                if (args.length === 0) {
                    output_el.innerText += JSON.stringify(data["ast"], null, 2);
                } else if (args.length === 2 && args[0] == ">") {
                    output_el.innerText += downloadJson(data, args[1]);
                } else {
                    output_el.innerText += "ERROR: unrecognized arguments for `ast`"
                }
            });

            return "&#9203;";
        }
        ,
        check: (args) => {
            Module().then(instance => {
                const check = instance.cwrap('check', 'number', ['string']);
                let ptr = check(editor.getValue());
                let data = instance.UTF8ToString(ptr);
                data = JSON.parse(data);

                if (args.length === 0) {
                    output_el.innerText += JSON.stringify(data, null, 2);
                } else if (args.length === 2 && args[0] == ">") {
                    output_el.innerText += downloadJson(data, args[1]);
                } else {
                    output_el.innerText += "ERROR: unrecognized arguments for `ast`"
                }
            });
            return "&#9203;";
        },
        ir: (args) => {
            Module().then(instance => {
                const compile = instance.cwrap('compile', 'number', ['string']);
                let ptr = compile(editor.getValue());
                let data = instance.UTF8ToString(ptr);
                data = JSON.parse(data);

                if (args.length === 0) {
                    output_el.innerText += data["ir"];
                } else if (args.length === 2 && args[0] == ">") {
                    downloadText(data["ir"], args[1]);
                } else {
                    output_el.innerText += "ERROR: unrecognized arguments for `ir`";
                }
            });
            return "&#9203;";
        },
        run: (args) => {
            Module().then(instance => {
                const compile_wat = instance.cwrap('compile_wat', 'number', ['string']);
                let ptr = compile_wat(editor.getValue());
                let data = instance.UTF8ToString(ptr);
                data = JSON.parse(data);

                const wat = data["wat"];
                if (!wat) {
                    output_el.innerText += "ERROR: compile_wat returned no WAT\n";
                    if (data["logs"]) output_el.innerText += JSON.stringify(data["logs"], null, 2);
                    return;
                }

                if (args.length === 2 && args[0] == ">") {
                    downloadText(wat, args[1]);
                    return;
                }

                // Use wabt.js to assemble WAT → WASM and run it
                WabtModule().then(wabt => {
                    let wasmModule;
                    try {
                        const parsed = wabt.parseWat("oberon.wat", wat, {});
                        parsed.validate();
                        const binary = parsed.toBinary({});
                        wasmModule = binary.buffer;
                    } catch (e) {
                        output_el.innerText += "WAT assembly error: " + e.message + "\n";
                        return;
                    }

                    WebAssembly.instantiate(wasmModule, {}).then(result => {
                        const exports = result.instance.exports;
                        if (exports.main) exports.main();
                        // Print all exported i64/i32 globals
                        output_el.innerText += "--- exported globals ---\n";
                        for (const [name, val] of Object.entries(exports)) {
                            if (val && typeof val === 'object' && 'value' in val) {
                                output_el.innerText += name + " = " + val.value + "\n";
                            }
                        }
                    }).catch(e => {
                        output_el.innerText += "WASM instantiation error: " + e.message + "\n";
                    });
                });
            });
            return "&#9203;";
        },
        clear: () => {
            output_el.innerText = "";
            return "";
        },
    };
    return commands;
}

function getCode() {
    return `(* Oberon-0 implementations of various sort algorithms. *)
MODULE Sort0;

(* Length of the array to be sorted. *)
CONST Dim = 2 * 10;

(* Array type *)
TYPE INTARRAY = ARRAY Dim OF INTEGER;

(* Array to be sorted. *)
VAR a: INTARRAY;

(* Initializes the array. *)
PROCEDURE Init();
VAR i: INTEGER;
BEGIN
    i := 0;
    WHILE i < Dim DO
        a[i] := Dim-i;
        i := i + 1
    END
END Init;

(* Swaps the two values passed as var-parameters. *)
PROCEDURE Swap(VAR a, b: INTEGER);
VAR t: INTEGER;
BEGIN
    t := a;
    a := b; 
    b := t
END Swap;

(* Applies the insertion-sort algorithm the the global array. *)
PROCEDURE InsertionSort;
VAR i, j: INTEGER;
BEGIN
    i := 1;
    WHILE i < Dim DO
        j := i;
        WHILE (j > 0) & (a[j - 1] > a[j]) DO
            Swap(a[j], a[j - 1]);
            j := j - 1
        END;
        i := i + 1
    END
END InsertionSort;

(* Applies the selection-sort algorithm the the global array. *)
PROCEDURE SelectionSort;
VAR i, j: INTEGER;
    min: INTEGER;
BEGIN
    i := 0;
    WHILE i < Dim - 1 DO
        min := i;
        j := i + 1;
        WHILE  j < Dim DO
            IF a[j] < a[min] THEN min := j END;
            j := j + 1
        END;
        IF min # i THEN Swap(a[i], a[min]) END;
        i := i + 1
    END
END SelectionSort;
    
(* Applies the bubble-sort algorithm to the global array. *)
PROCEDURE BubbleSort;
VAR i, j: INTEGER;
BEGIN
    i := 0;
    WHILE i < Dim DO
        j := Dim - 1;
        WHILE j > i DO
            IF a[j - 1] > a[j] THEN Swap(a[j - 1], a[j]) END;
            j := j - 1
        END;
        i := i + 1
    END
END BubbleSort;


(* Applies the quick-sort algorithm to the global array. *)
PROCEDURE QuickSort;

    PROCEDURE QSort(l, r: INTEGER);
    VAR i, j, x: INTEGER;
    BEGIN
        i := l;
        j := r;
        x := a[(r + l) DIV 2];
        WHILE i <= j DO
            WHILE a[i] < x DO i := i + 1 END;
            WHILE x < a[j] DO j := j - 1 END;
            IF i <= j THEN
                Swap(a[i], a[j]);
                i := i + 1;
                j := j - 1
            END
        END;
        IF l < j THEN QSort(l, j) END;
        IF i < r THEN QSort(i, r) END
    END QSort;

BEGIN
    QSort(0, Dim - 1)
END QuickSort;

(* Main program. *)
BEGIN
    Init;
    QuickSort
END Sort0.`;
}


