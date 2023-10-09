#include <ux/app.hpp>

using namespace ion;


bool isIdentifier(str token) {
	RegEx regex("[\\w\\.:]+/");
	return token && regex.exec(token);
}

struct TokenTypeMatcher {
	Matcher matcher;
	StandardTokenType type;
};

struct ITokenizeLineResult {
	array<IToken> tokens;
	/**
	 * The `prevState` to be passed on to the next line tokenization.
	 */
	StateStack ruleStack;
	/**
	 * Did tokenization stop early due to reaching the time limit.
	 */
	bool stoppedEarly;
};

struct ITokenizeLineResult2 {
	/**
	 * The tokens in binary format. Each token occupies two array indices. For token i:
	 *  - at offset 2*i => startIndex
	 *  - at offset 2*i + 1 => metadata
	 *
	 */
	Uint32Array tokens;
	/**
	 * The `prevState` to be passed on to the next line tokenization.
	 */
	StateStack ruleStack;
	/**
	 * Did tokenization stop early due to reaching the time limit.
	 */
	bool stoppedEarly;
};

struct IToken {
	num startIndex;
	num endIndex;
	array<str> scopes;
};

struct ILineTokensResult {
	num lineLength;
	LineTokens lineTokens;
	StateStackImpl ruleStack;
	bool stoppedEarly;
};

struct LineTokens {

	boolean _emitBinaryTokens: ;
	/**
	 * defined only if `DebugFlags.InDebugMode`.
	 */
	string _lineText;
	/**
	 * used only if `_emitBinaryTokens` is false.
	 */
	array<IToken> _tokens;
	/**
	 * used only if `_emitBinaryTokens` is true.
	 */
	array<num> _binaryTokens;
	num _lastTokenEndIndex;
	array<TokenTypeMatcher> _tokenTypeOverrides;

	constructor(
		emitBinaryTokens: boolean,
		lineText: string,
		tokenTypeOverrides: TokenTypeMatcher[],
		private readonly balancedBracketSelectors: BalancedBracketSelectors | null,
	) {
		this._emitBinaryTokens = emitBinaryTokens;
		this._tokenTypeOverrides = tokenTypeOverrides;
		if (DebugFlags.InDebugMode) {
			this._lineText = lineText;
		} else {
			this._lineText = null;
		}
		this._tokens = [];
		this._binaryTokens = [];
		this._lastTokenEndIndex = 0;
	}

	public produce(stack: StateStackImpl, endIndex: number): void {
		this.produceFromScopes(stack.contentNameScopesList, endIndex);
	}

	public produceFromScopes(
		scopesList: AttributedScopeStack | null,
		endIndex: number
	): void {
		if (this._lastTokenEndIndex >= endIndex) {
			return;
		}

		if (this._emitBinaryTokens) {
			let metadata = scopesList?.tokenAttributes ?? 0;
			let containsBalancedBrackets = false;
			if (this.balancedBracketSelectors?.matchesAlways) {
				containsBalancedBrackets = true;
			}

			if (this._tokenTypeOverrides.length > 0 || (this.balancedBracketSelectors && !this.balancedBracketSelectors.matchesAlways && !this.balancedBracketSelectors.matchesNever)) {
				// Only generate scope array when required to improve performance
				const scopes = scopesList?.getScopeNames() ?? [];
				for (const tokenType of this._tokenTypeOverrides) {
					if (tokenType.matcher(scopes)) {
						metadata = EncodedTokenAttributes.set(
							metadata,
							0,
							toOptionalTokenType(tokenType.type),
							null,
							FontStyle.NotSet,
							0,
							0
						);
					}
				}
				if (this.balancedBracketSelectors) {
					containsBalancedBrackets = this.balancedBracketSelectors.match(scopes);
				}
			}

			if (containsBalancedBrackets) {
				metadata = EncodedTokenAttributes.set(
					metadata,
					0,
					OptionalStandardTokenType.NotSet,
					containsBalancedBrackets,
					FontStyle.NotSet,
					0,
					0
				);
			}

			if (this._binaryTokens.length > 0 && this._binaryTokens[this._binaryTokens.length - 1] === metadata) {
				// no need to push a token with the same metadata
				this._lastTokenEndIndex = endIndex;
				return;
			}

			if (DebugFlags.InDebugMode) {
				const scopes = scopesList?.getScopeNames() ?? [];
				console.log('  token: |' + this._lineText!.substring(this._lastTokenEndIndex, endIndex).replace(/\n$/, '\\n') + '|');
				for (let k = 0; k < scopes.length; k++) {
					console.log('      * ' + scopes[k]);
				}
			}

			this._binaryTokens.push(this._lastTokenEndIndex);
			this._binaryTokens.push(metadata);

			this._lastTokenEndIndex = endIndex;
			return;
		}

		const scopes = scopesList?.getScopeNames() ?? [];

		if (DebugFlags.InDebugMode) {
			console.log('  token: |' + this._lineText!.substring(this._lastTokenEndIndex, endIndex).replace(/\n$/, '\\n') + '|');
			for (let k = 0; k < scopes.length; k++) {
				console.log('      * ' + scopes[k]);
			}
		}

		this._tokens.push({
			startIndex: this._lastTokenEndIndex,
			endIndex: endIndex,
			// value: lineText.substring(lastTokenEndIndex, endIndex),
			scopes: scopes
		});

		this._lastTokenEndIndex = endIndex;
	}

	public getResult(stack: StateStackImpl, lineLength: number): IToken[] {
		if (this._tokens.length > 0 && this._tokens[this._tokens.length - 1].startIndex === lineLength - 1) {
			// pop produced token for newline
			this._tokens.pop();
		}

		if (this._tokens.length === 0) {
			this._lastTokenEndIndex = -1;
			this.produce(stack, lineLength);
			this._tokens[this._tokens.length - 1].startIndex = 0;
		}

		return this._tokens;
	}

	public getBinaryResult(stack: StateStackImpl, lineLength: number): Uint32Array {
		if (this._binaryTokens.length > 0 && this._binaryTokens[this._binaryTokens.length - 2] === lineLength - 1) {
			// pop produced token for newline
			this._binaryTokens.pop();
			this._binaryTokens.pop();
		}

		if (this._binaryTokens.length === 0) {
			this._lastTokenEndIndex = -1;
			this.produce(stack, lineLength);
			this._binaryTokens[this._binaryTokens.length - 2] = 0;
		}

		const result = new Uint32Array(this._binaryTokens.length);
		for (let i = 0, len = this._binaryTokens.length; i < len; i++) {
			result[i] = this._binaryTokens[i];
		}

		return result;
	}
}

TokenizeStringResult _tokenizeString(
	Grammar grammar, str lineText, bool isFirstLine,
	num linePos, StateStackImpl stack, LineTokens lineTokens,
	bool checkWhileConditions, num timeLimit 
);

struct Grammar:mx { // implements IGrammar, IRuleFactoryHelper, IOnigLib

	struct members {
		RuleId 					_rootId = -1;
		number 					_lastRuleId;
		array<Rule> 			_ruleId2desc;
		var 					_grammar;
		array<TokenTypeMatcher> _tokenTypeMatchers;
	};

	mx_object(Grammar, mx, members);

	/// remove Repository
	/// remove Scope/Theme
	Grammar(
		var grammar,
		ITokenTypeMap tokenTypes,
		BalancedBracketSelectors balancedBracketSelectors,
	) {
		this._rootId = -1;
		this._lastRuleId = 0;
		this._ruleId2desc = [null];
		this._includedGrammars = {};
		this._grammar = grammar;

		this._tokenTypeMatchers = [];
		if (tokenTypes) {
			for (const selector of Object.keys(tokenTypes)) {
				const matchers = createMatchers(selector, nameMatcher);
				for (const matcher of matchers) {
					this._tokenTypeMatchers.push({
						matcher: matcher.matcher,
						type: tokenTypes[selector],
					});
				}
			}
		}
	}

	void dispose() {
		for (auto &rule: this._ruleId2desc) {
			if (rule) {
				rule.dispose();
			}
		}
	}

	Rule registerRule(lambda<Rule(RuleId)> factory) {
		const id = ++this._lastRuleId;
		const result = factory(ruleIdFromNumber(id));
		this._ruleId2desc[id] = result; /// this cant be set this way
		return result;
	}

	Rule getRule(RuleId ruleId) {
		return this._ruleId2desc[int(ruleId)];
	}

	ITokenizeLineResult tokenizeLine(
			str lineText,
			StateStackImpl prevState,
			num timeLimit = 0) {
		const r = this._tokenize(lineText, prevState, false, timeLimit);
		return {
			tokens: 		r.lineTokens.getResult(r.ruleStack, r.lineLength),
			ruleStack: 		r.ruleStack,
			stoppedEarly: 	r.stoppedEarly,
		};
	}

	ITokenizeLineResult2 tokenizeLine2(
			str 			lineText,
			StateStackImpl 	prevState,
			num 			timeLimit = 0) {
		const r = this._tokenize(lineText, prevState, true, timeLimit);
		return {
			tokens: 		r.lineTokens.getBinaryResult(r.ruleStack, r.lineLength),
			ruleStack: 		r.ruleStack,
			stoppedEarly: 	r.stoppedEarly,
		};
	}

	ILineTokensResult _tokenize(
		str lineText,
		StateStackImpl prevState,
		bool emitBinaryTokens,
		num timeLimit
	) {
		if (this._rootId === -1) {
			this._rootId = RuleFactory.getCompiledRuleId(
				this._grammar.repository.$self,
				this,
				this._grammar.repository
			);
			// This ensures ids are deterministic, and thus equal in renderer and webworker.
			this.getInjections();
		}

		bool isFirstLine;
		if (!prevState || prevState === StateStackImpl.NULL) {
			isFirstLine = true;
			const rawDefaultMetadata =
				this._basicScopeAttributesProvider.getDefaultAttributes();
			const defaultStyle = this.themeProvider.getDefaults();
			const defaultMetadata = EncodedTokenAttributes.set(
				0,
				rawDefaultMetadata.languageId,
				rawDefaultMetadata.tokenType,
				null,
				defaultStyle.fontStyle,
				defaultStyle.foregroundId,
				defaultStyle.backgroundId
			);

			const rootScopeName = this.getRule(this._rootId).getName(
				null,
				null
			);

			AttributedScopeStack scopeList;
			if (rootScopeName) {
				scopeList = AttributedScopeStack.createRootAndLookUpScopeName(
					rootScopeName,
					defaultMetadata,
					this
				);
			} else {
				scopeList = AttributedScopeStack.createRoot(
					 "unknown",
					 defaultMetadata
				);
			}

			prevState = new StateStackImpl(
				null,
				this._rootId,
				-1,
				-1,
				false,
				null,
				scopeList,
				scopeList
			);
		} else {
			isFirstLine = false;
			prevState.reset();
		}

		lineText += "\n";
		num   lineLength = lineText.len()
		const lineTokens = LineTokens(
			emitBinaryTokens,
			lineText,
			this._tokenTypeMatchers,
			this.balancedBracketSelectors
		);
		const r = _tokenizeString(
			this,
			onigLineText,
			isFirstLine,
			0,
			prevState,
			lineTokens,
			true,
			timeLimit
		);
		return ILineTokensResult {
			lineLength	 = lineLength,
			lineTokens	 = lineTokens,
			ruleStack	 = r.stack,
			stoppedEarly = r.stoppedEarly,
		};
	}
};




IWhileCheckResult _checkWhileConditions(
		Grammar &grammar, str &lineText, bool isFirstLine,
		num linePos, StateStackImpl stack, LineTokens &lineTokens) {
	num anchorPosition = (stack.beginRuleCapturedEOL ? 0 : -1);
	struct IWhileStack {
		StateStackImpl stack;
		BeginWhileRule rule;
	};
	array<IWhileStack> whileRules;

	for (StateStackImpl node = stack; node; node = node.pop()) {
		mx nodeRule = node.getRule(grammar);
		if (nodeRule.type() == typeof(BeginWhileRule)) {
			whileRules.push(IWhileStack {
				rule  = nodeRule,
				stack = node
			});
		}
	}

	for (let whileRule = whileRules.pop(); whileRule; whileRule = whileRules.pop()) {
		const { ruleScanner, findOptions } = prepareRuleWhileSearch(whileRule.rule, grammar, whileRule.stack.endRule, isFirstLine, linePos === anchorPosition);
		const r = ruleScanner.findNextMatchSync(lineText, linePos, findOptions);
		if (DebugFlags.InDebugMode) {
			console.log('  scanning for while rule');
			console.log(ruleScanner.toString());
		}

		if (r) {
			const matchedRuleId = r.ruleId;
			if (matchedRuleId !== whileRuleId) {
				// we shouldn't end up here
				stack = whileRule.stack.pop()!;
				break;
			}
			if (r.captureIndices && r.captureIndices.length) {
				lineTokens.produce(whileRule.stack, r.captureIndices[0].start);
				handleCaptures(grammar, lineText, isFirstLine, whileRule.stack, lineTokens, whileRule.rule.whileCaptures, r.captureIndices);
				lineTokens.produce(whileRule.stack, r.captureIndices[0].end);
				anchorPosition = r.captureIndices[0].end;
				if (r.captureIndices[0].end > linePos) {
					linePos = r.captureIndices[0].end;
					isFirstLine = false;
				}
			}
		} else {
			if (DebugFlags.InDebugMode) {
				console.log('  popping ' + whileRule.rule.debugName + ' - ' + whileRule.rule.debugWhileRegExp);
			}

			stack = whileRule.stack.pop()!;
			break;
		}
	}

	return { stack: stack, linePos: linePos, anchorPosition: anchorPosition, isFirstLine: isFirstLine };
}




TokenizeStringResult _tokenizeString(
	Grammar grammar, str lineText, bool isFirstLine,
	num linePos, StateStackImpl stack, LineTokens lineTokens,
	bool checkWhileConditions, num timeLimit 
) {
	num lineLength = lineText.len();

	bool STOP = false;
	num anchorPosition = -1;

	if (checkWhileConditions) {
		const whileCheckResult = _checkWhileConditions(
			grammar,
			lineText,
			isFirstLine,
			linePos,
			stack,
			lineTokens
		);
		stack = whileCheckResult.stack;
		linePos = whileCheckResult.linePos;
		isFirstLine = whileCheckResult.isFirstLine;
		anchorPosition = whileCheckResult.anchorPosition;
	}

	const startTime = Date.now();
	while (!STOP) {
		if (timeLimit !== 0) {
			const elapsedTime = Date.now() - startTime;
			if (elapsedTime > timeLimit) {
				return new TokenizeStringResult(stack, true);
			}
		}
		scanNext(); // potentially modifies linePos && anchorPosition
	}

	return new TokenizeStringResult(stack, false);

	function scanNext(): void {
		if (DebugFlags.InDebugMode) {
			console.log("");
			console.log(
				`@@scanNext ${linePos}: |${lineText.content
					.substr(linePos)
					.replace(/\n$/, "\\n")}|`
			);
		}
		const r = matchRuleOrInjections(
			grammar,
			lineText,
			isFirstLine,
			linePos,
			stack,
			anchorPosition
		);

		if (!r) {
			if (DebugFlags.InDebugMode) {
				console.log("  no more matches.");
			}
			// No match
			lineTokens.produce(stack, lineLength);
			STOP = true;
			return;
		}

		const captureIndices: IOnigCaptureIndex[] = r.captureIndices;
		const matchedRuleId = r.matchedRuleId;

		const hasAdvanced =
			captureIndices && captureIndices.length > 0
				? captureIndices[0].end > linePos
				: false;

		if (matchedRuleId === endRuleId) {
			// We matched the `end` for this rule => pop it
			const poppedRule = <BeginEndRule>stack.getRule(grammar);

			if (DebugFlags.InDebugMode) {
				console.log(
					"  popping " +
						poppedRule.debugName +
						" - " +
						poppedRule.debugEndRegExp
				);
			}

			lineTokens.produce(stack, captureIndices[0].start);
			stack = stack.withContentNameScopesList(stack.nameScopesList!);
			handleCaptures(
				grammar,
				lineText,
				isFirstLine,
				stack,
				lineTokens,
				poppedRule.endCaptures,
				captureIndices
			);
			lineTokens.produce(stack, captureIndices[0].end);

			// pop
			const popped = stack;
			stack = stack.parent!;
			anchorPosition = popped.getAnchorPos();

			if (!hasAdvanced && popped.getEnterPos() === linePos) {
				// Grammar pushed & popped a rule without advancing
				if (DebugFlags.InDebugMode) {
					console.error(
						"[1] - Grammar is in an endless loop - Grammar pushed & popped a rule without advancing"
					);
				}

				// See https://github.com/Microsoft/vscode-textmate/issues/12
				// Let's assume this was a mistake by the grammar author and the intent was to continue in this state
				stack = popped;

				lineTokens.produce(stack, lineLength);
				STOP = true;
				return;
			}
		} else {
			// We matched a rule!
			const _rule = grammar.getRule(matchedRuleId);

			lineTokens.produce(stack, captureIndices[0].start);

			const beforePush = stack;
			// push it on the stack rule
			const scopeName = _rule.getName(lineText.content, captureIndices);
			const nameScopesList = stack.contentNameScopesList!.pushAttributed(
				scopeName,
				grammar
			);
			stack = stack.push(
				matchedRuleId,
				linePos,
				anchorPosition,
				captureIndices[0].end === lineLength,
				null,
				nameScopesList,
				nameScopesList
			);

			if (_rule instanceof BeginEndRule) {
				const pushedRule = _rule;
				if (DebugFlags.InDebugMode) {
					console.log(
						"  pushing " +
							pushedRule.debugName +
							" - " +
							pushedRule.debugBeginRegExp
					);
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					pushedRule.beginCaptures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);
				anchorPosition = captureIndices[0].end;

				const contentName = pushedRule.getContentName(
					lineText.content,
					captureIndices
				);
				const contentNameScopesList = nameScopesList.pushAttributed(
					contentName,
					grammar
				);
				stack = stack.withContentNameScopesList(contentNameScopesList);

				if (pushedRule.endHasBackReferences) {
					stack = stack.withEndRule(
						pushedRule.getEndWithResolvedBackReferences(
							lineText.content,
							captureIndices
						)
					);
				}

				if (!hasAdvanced && beforePush.hasSameRuleAs(stack)) {
					// Grammar pushed the same rule without advancing
					if (DebugFlags.InDebugMode) {
						console.error(
							"[2] - Grammar is in an endless loop - Grammar pushed the same rule without advancing"
						);
					}
					stack = stack.pop()!;
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			} else if (_rule instanceof BeginWhileRule) {
				const pushedRule = <BeginWhileRule>_rule;
				if (DebugFlags.InDebugMode) {
					console.log("  pushing " + pushedRule.debugName);
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					pushedRule.beginCaptures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);
				anchorPosition = captureIndices[0].end;
				const contentName = pushedRule.getContentName(
					lineText.content,
					captureIndices
				);
				const contentNameScopesList = nameScopesList.pushAttributed(
					contentName,
					grammar
				);
				stack = stack.withContentNameScopesList(contentNameScopesList);

				if (pushedRule.whileHasBackReferences) {
					stack = stack.withEndRule(
						pushedRule.getWhileWithResolvedBackReferences(
							lineText.content,
							captureIndices
						)
					);
				}

				if (!hasAdvanced && beforePush.hasSameRuleAs(stack)) {
					// Grammar pushed the same rule without advancing
					if (DebugFlags.InDebugMode) {
						console.error(
							"[3] - Grammar is in an endless loop - Grammar pushed the same rule without advancing"
						);
					}
					stack = stack.pop()!;
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			} else {
				const matchingRule = <MatchRule>_rule;
				if (DebugFlags.InDebugMode) {
					console.log(
						"  matched " +
							matchingRule.debugName +
							" - " +
							matchingRule.debugMatchRegExp
					);
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					matchingRule.captures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);

				// pop rule immediately since it is a MatchRule
				stack = stack.pop()!;

				if (!hasAdvanced) {
					// Grammar is not advancing, nor is it pushing/popping
					if (DebugFlags.InDebugMode) {
						console.error(
							"[4] - Grammar is in an endless loop - Grammar is not advancing, nor is it pushing/popping"
						);
					}
					stack = stack.safePop();
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			}
		}

		if (captureIndices[0].end > linePos) {
			// Advance stream
			linePos = captureIndices[0].end;
			isFirstLine = false;
		}
	}
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

using Matcher = lambda<bool(mx)>;
/*
struct Matcher:mx {
	struct vars {
		lambda<bool(mx)> matcherInput; /// matcherInput = lambda
	};
	mx_object(Matcher, mx, vars);
	Matcher(lambda<bool(mx)> matcherInput) : Matcher() {
		data->matcherInput = matcherInput;
	}
};
*/

struct MatcherWithPriority:mx {
	struct vars {
		Matcher matcher;
		num 	priority; // -1, 0, 1
	};
	mx_object(MatcherWithPriority, mx, vars);
	MatcherWithPriority(Matcher &matcher, num priority) : MatcherWithPriority() {
		data->matcher  = matcher;
		data->priority = priority;
	}
};


using NameMatcher = lambda<bool(array<str>&, mx&)>;

array<MatcherWithPriority> createMatchers(str selector, NameMatcher matchesName) {
	array<MatcherWithPriority> results;
	Tokenizer tokenizer { selector };
	str token = tokenizer->next();

	/// accesses tokenizer
	lambda<Matcher()> parseInnerExpression;
	lambda<Matcher()> &parseInnerExpressionRef = parseInnerExpression;
	lambda<Matcher()> parseOperand;
	lambda<Matcher()> &parseOperandRef = parseOperand; /// dont believe we can merely set after we copy the other value in; the data will change
	parseOperand = [&token, _tokenizer=tokenizer, matchesName, parseInnerExpressionRef, parseOperandRef]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		if (token == "-") {
			token = tokenizer->next();
			Matcher expressionToNegate = parseOperandRef();
			return Matcher([expressionToNegate](mx matcherInput) -> bool {
				return bool(expressionToNegate) && !expressionToNegate(matcherInput);
			});
		}
		if (token == "(") {
			token = tokenizer->next();
			Matcher expressionInParents = parseInnerExpressionRef();
			if (token == ")") {
				token = tokenizer->next();
			}
			return expressionInParents;
		}
		if (isIdentifier(token)) {
			array<str> identifiers;
			do {
				identifiers.push(token);
				token = tokenizer->next();
			} while (isIdentifier(token));
			///
			return Matcher([matchesName, identifiers] (mx matcherInput) -> bool {
				return matchesName((array<str>&)identifiers, matcherInput);
			});
		}
		return null;
	};

	/// accesses tokenizer
	auto parseConjunction = [_tokenizer=tokenizer, parseOperand]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		array<Matcher> matchers;
		Matcher matcher = parseOperand();
		while (matcher) {
			matchers.push(matcher);
			matcher = parseOperand();
		}
		return [matchers](mx matcherInput) -> bool {
			return matchers.every([matcherInput](Matcher &matcher) -> bool {
				return matcher(matcherInput);
			});
		};
	};

	/// accesses tokenizer
	parseInnerExpression = [&token, _tokenizer=tokenizer, parseConjunction]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		array<Matcher> matchers;
		Matcher matcher = parseConjunction();
		while (matcher) {
			matchers.push(matcher);
			if (token == "|" || token == ",") {
				do {
					token = tokenizer->next();
				} while (token == "|" || token == ","); // ignore subsequent commas
			} else {
				break;
			}
			matcher = parseConjunction();
		}
		return Matcher([matchers](mx matcherInput) -> bool {
			return matchers.some([matcherInput](Matcher &matcher) {
				return matcher(matcherInput);
			});
		});
	};

	/// top level loop to iterate through tokens
	while (token) {
		num priority = 0;
		if (token.len() == 2 && token[1] == ':') {
			switch (token[0]) {
				case 'R': priority =  1; break;
				case 'L': priority = -1; break;
				default:
					console.log("Unknown priority {0} in scope selector", {token});
					return {};
			}
			token = tokenizer->next();
		}
		Matcher matcher = parseConjunction();
		results.push(MatcherWithPriority { matcher, priority });
		if (token != ",") {
			break;
		}
		token = tokenizer->next();
	}
	return results;
}

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
