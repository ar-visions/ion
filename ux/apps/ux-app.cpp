#include <ux/app.hpp>

using namespace ion;


bool isIdentifier(str token) {
	RegEx regex("[\\w\\.:]+/");
	return token && regex.exec(token);
}

struct Tokenizer:mx {
    struct state {
		str			  input;
		RegEx		  regex;
		array<str>    match;
		str next() {
			if (!match)
				return {};
			str prev = match[0];
			match = regex.exec(input); /// regex has a marker for its last match stored, so one can call it again if you give it the same input.  if its a different input it will reset
			return prev;
		}
    };

    mx_object(Tokenizer, mx, state);

    Tokenizer(str input) : Tokenizer() {
        data->regex = str("([LR]:|[\\w\\.:][\\w\\.:\\-]*|[\\,\\|\\-\\(\\)])");
	    data->match = data->regex.exec(input);
    }
};

struct Matcher:mx {
	struct vars {
		mx matcherInput;
	};
	mx_object(Matcher, mx, vars);
}
/*

interface MatcherWithPriority<T> {
	matcher: Matcher<T>;
	priority: -1 | 0 | 1;
}

export interface Matcher<T> {
	(matcherInput: T): boolean;
}

export function createMatchers<T>(selector: string, matchesName: (names: string[], matcherInput: T) => boolean): MatcherWithPriority<T>[] {
	const results = <MatcherWithPriority<T>[]>[];
	const tokenizer = newTokenizer(selector);
	let token = tokenizer.next();
	while (token !== null) {
		let priority: -1 | 0 | 1 = 0;
		if (token.length === 2 && token.charAt(1) === ':') {
			switch (token.charAt(0)) {
				case 'R': priority = 1; break;
				case 'L': priority = -1; break;
				default:
					console.log(`Unknown priority ${token} in scope selector`);
			}
			token = tokenizer.next();
		}
		let matcher = parseConjunction();
		results.push({ matcher, priority });
		if (token !== ',') {
			break;
		}
		token = tokenizer.next();
	}
	return results;

	function parseOperand(): Matcher<T> | null {
		if (token === '-') {
			token = tokenizer.next();
			const expressionToNegate = parseOperand();
			return matcherInput => !!expressionToNegate && !expressionToNegate(matcherInput);
		}
		if (token === '(') {
			token = tokenizer.next();
			const expressionInParents = parseInnerExpression();
			if (token === ')') {
				token = tokenizer.next();
			}
			return expressionInParents;
		}
		if (isIdentifier(token)) {
			const identifiers: string[] = [];
			do {
				identifiers.push(token);
				token = tokenizer.next();
			} while (isIdentifier(token));
			return matcherInput => matchesName(identifiers, matcherInput);
		}
		return null;
	}
	function parseConjunction(): Matcher<T> {
		const matchers: Matcher<T>[] = [];
		let matcher = parseOperand();
		while (matcher) {
			matchers.push(matcher);
			matcher = parseOperand();
		}
		return matcherInput => matchers.every(matcher => matcher(matcherInput)); // and
	}
	function parseInnerExpression(): Matcher<T> {
		const matchers: Matcher<T>[] = [];
		let matcher = parseConjunction();
		while (matcher) {
			matchers.push(matcher);
			if (token === '|' || token === ',') {
				do {
					token = tokenizer.next();
				} while (token === '|' || token === ','); // ignore subsequent commas
			} else {
				break;
			}
			matcher = parseConjunction();
		}
		return matcherInput => matchers.some(matcher => matcher(matcherInput)); // or
	}
}


*/

struct View:Element {
    struct props {
        int         sample;
        int         sample2;
        int         test_set;
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",   sample   },
                prop { "sample2",  sample2  },
                prop { "test_set", test_set },
                prop { "clicked",  clicked  }
            };
        }
        type_register(props);
    };

    component(View, Element, props);

    node update() {
        return array<node> {
            Edit {
                { "content", "Multiline edit test" }
            }
        };
    }
};

int main(int argc, char *argv[]) {
	RegEx 	regex("\\w+");
    array<str> matches = regex.exec("Hello, World!", RegEx::Behaviour::global);

	if (matches)
		console.log("match: {0}", {matches[0]});

	path js 	 = "style/js.json";
	num  m0 	 = millis();
	var  grammar = js.read<var>();
	num  m1  	 = millis();

	console.log("millis = {0}", {m1-m0});
    
    map<mx> defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map<mx> config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    ///
    return App(config, [](App &app) -> node {
        return View {
            { "id",      "main" },
            { "sample",  int(2) },
            { "sample2", "10"   },
            { "on-hover", callback([](event e) {
                console.log("hi");
            }) },
            { "clicked", callback([](event e) {
                printf("test!\n");
            }) }
        };
    });
}
