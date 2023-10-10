#include <ux/app.hpp>

using namespace ion;


#if 0

import { RegexSource, mergeObjects, basename, escapeRegExpCharacters, OrMask } from './utils';

import { IOnigLib, OnigScanner, IOnigCaptureIndex, FindOption, IOnigMatch, OnigString } from './onigLib';
import { ILocation, IRawGrammar, IRawRepository, IRawRule, IRawCaptures } from './rawGrammar';
import { IncludeReferenceKind, parseInclude } from './grammar/grammarDependencies';

const RegEx16 HAS_BACK_REFERENCES  = RegEx16(R("\\(\d+)"), RegEx::Behaviour::none);
const RegEx16 BACK_REFERENCING_END = RegEx16(R("\\(\d+)"), RegEx::Behaviour::global);

enums(IncludeReferenceKind, Base,
	Base,
	Self,
	RelativeReference,
	TopLevelReference,
	TopLevelRepositoryReference);

struct IncludeReference:mx {
	struct members {
		IncludeReferenceKind kind;
		str ruleName;
		str scopeName; // not needed
	};
	IncludeReference(IncludeReferenceKind kind) : IncludeReference() {
		data->kind = kind;
	}
	mx_object(IncludeReference, mx, members);
};

struct BaseReference:IncludeReference {
	BaseReference() : IncludeReference(IncludeReferenceKind::Base) { }
};

struct SelfReference:IncludeReference {
	SelfReference() : IncludeReference(IncludeReferenceKind::Self) { }
};

struct RelativeReference:IncludeReference {
	RelativeReference(str ruleName) : 
			IncludeReference(IncludeReferenceKind::RelativeReference) {
		IncludeReference::data->ruleName = ruleName;
	}
};

struct TopLevelReference:IncludeReference {
	RelativeReference(str scopeName) : 
			IncludeReference(IncludeReferenceKind::TopLevelReference) {
		IncludeReference::data->scopeName = scopeName;
	}
};

struct TopLevelRepositoryReference:IncludeReference {
	TopLevelRepositoryReference(str scopeName, str ruleName) : 
			IncludeReference(IncludeReferenceKind::TopLevelRepositoryReference) {
		IncludeReference::data->scopeName = scopeName;
		IncludeReference::data->ruleName = ruleName;
	}
};

IncludeReference parseInclude(utf8 include) {
	if (include == "$base") {
		return BaseReference();
	} else if (include == "$self") {
		return SelfReference();
	}

	const indexOfSharp = include.indexOf("#");
	if (indexOfSharp == -1) {
		return TopLevelReference(include);
	} else if (indexOfSharp == 0) {
		return RelativeReference(include.mid(1));
	} else {
		const scopeName = include.mid(0, indexOfSharp);
		const ruleName = include.mid(indexOfSharp + 1);
		return TopLevelRepositoryReference(scopeName, ruleName);
	}
}

struct ILocation {
	str 			filename;
	num 			line;
	num 			chr;
};

struct ILocatable {
	ILocation 		_vscodeTextmateLocation;
};

struct IRawRepositoryMap {
	map<mx> 		props; /// of type IRawRule
	IRawRule 		_self;
	IRawRule 		_base;
};

/// I can be both Implement type and Interface, in mx use-case
/// if a context type implements it, it will contain the type
/// you can also use it internal to implement

export type IRawCaptures = IRawCapturesMap & ILocatable;

struct IRawRule:mx {
	struct members {
		RuleId 				id; // This is not part of the spec only used internally
		str 				include;
		utf16 				name;
		utf16 				contentName;
		utf16 				match;
		IRawCaptures 		captures;
		utf16 				begin;
		IRawCaptures	 	beginCaptures;
		utf16 				end;
		IRawCaptures 		endCaptures;
		utf16 				_while;
		IRawCaptures 		whileCaptures;
		array<IRawRule>	 	patterns;
	  //IRawRepository 		repository;
		bool 				applyEndPatternLast;
	};
	mx_object(IRawRule, mx, members);
};

struct IOnigCaptureIndex {
	num start;
	num end;
	num length;
};

struct IOnigMatch {
	num index;
	array<IOnigCaptureIndex> captureIndices;
};

enums(FindOption, None,
	None,
	NotBeginString,
	NotEndString,
	NotBeginPosition,
	DebugCall,
);

using RuleId = num;

// This is a special constant to indicate that the end regexp matched.
static const num endRuleId = -1;

// This is a special constant to indicate that the while regexp matched.
static const num whileRuleId = -2;

static RuleId ruleIdFromNumber(num id) { // this doesnt square with the above type def
	return id;
}

static num ruleIdToNumber(RuleId id) {
	return id;
}

struct IRuleRegistry {
	virtual Rule getRule(RuleId ruleId) = 0;
	virtual Rule registerRule(lambda<Rule(RuleId)> factory) = 0;
};

//struct IGrammarRegistry {
//	IRawGrammar getExternalGrammar(scopeName: sr, repository: IRawRepository);
//}

//struct IRuleFactoryHelper: IRuleRegistry, IGrammarRegistry {
//}

using IRuleFactoryHelper = IRuleRegistry;

struct com {
	com() {

	}
};

struct IData {
	int test;
};

struct IData2 {
	str test2;
};

struct test:com<IData, IData2> {
	/// i need the template var-arg for this; just something to go through those IStruct composition members
};

/// abstract
struct Rule:mx {
	struct members {
		ILocation _location;
		RuleId     id;
		bool 	  _nameIsCapturing;
		utf16  	  _name;
		bool 	  _contentNameIsCapturing;
		utf16  	  _contentName;
	}

	Rule(ILocation _location, RuleId id, utf16 name, utf16 contentName) : Rule() {
		data->_location = _location;
		data->id = id;
		data->_name = name || null;
		data->_nameIsCapturing = RegexSource.hasCaptures(data->_name);
		data->_contentName = contentName || null;
		data->_contentNameIsCapturing = RegexSource.hasCaptures(data->_contentName);
	}

	virtual void dispose() = 0;

	utf16 debugName() {
		if (data->_location)
			return fmt {"{0}:{1}", { basename(data->_location.filename), data->_location.line }};
		return "unknown";
	}

	utf16 getName(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
		if (!data->_nameIsCapturing || data->_name == null || lineText == null || captureIndices == null) {
			return data->_name;
		}
		return RegexSource.replaceCaptures(data->_name, lineText, captureIndices);
	}

	utf16 getContentName(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
		if (!data->_contentNameIsCapturing || data->_contentName == null) {
			return data->_contentName;
		}
		return RegexSource.replaceCaptures(data->_contentName, lineText, captureIndices);
	}

	virtual void collectPatternssgrammar: IRuleRegistry, out: RegExpSourceList) = 0;
	virtual CompiledRule compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16) = 0;
	virtual CompiledRule compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool) = 0;
}

struct ICompilePatternsResult {
	array<RuleId> patterns;
	bool hasMissingPatterns;
};

class CaptureRule : Rule {

	RuleId retokenizeCapturedWithRuleId;

	CaptureRule(ILocation _location, id: RuleId, name: utf16, contentName: utf16, retokenizeCapturedWithRuleId: RuleId) {
		super(_location, id, name, contentName);
		data->retokenizeCapturedWithRuleId = retokenizeCapturedWithRuleId;
	}

	public dispose(): void {
		// nothing to dispose
	}

	public collectPatterns(grammar: IRuleRegistry, out: RegExpSourceList) {
		throw new Error('Not supported!');
	}

	public compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule {
		throw new Error('Not supported!');
	}

	public compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule {
		throw new Error('Not supported!');
	}
}

export class MatchRule extends Rule {
	private readonly _match: RegExpSource;
	public readonly captures: CaptureRule[];
	private _cachedCompiledPatterns: RegExpSourceList;

	constructor(_location: ILocation, id: RuleId, name: utf16, match: utf16, captures: CaptureRule[]) {
		super(_location, id, name, null);
		data->_match = new RegExpSource(match, data->id);
		data->captures = captures;
		data->_cachedCompiledPatterns = null;
	}

	public dispose(): void {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
	}

	public get debugMatchRegExp(): utf16 {
		return `${data->_match.source}`;
	}

	public collectPatterns(grammar: IRuleRegistry, out: RegExpSourceList) {
		out.push(data->_match);
	}

	public compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	public compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	private _getCachedCompiledPatterns(grammar: IRuleRegistry & IOnigLib): RegExpSourceList {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();
			data->collectPatterns(grammar, data->_cachedCompiledPatterns);
		}
		return data->_cachedCompiledPatterns;
	}
}

export class IncludeOnlyRule extends Rule {
	public readonly hasMissingPatterns: bool;
	public readonly patterns: RuleId[];
	private _cachedCompiledPatterns: RegExpSourceList;

	constructor(_location: ILocation, id: RuleId, name: utf16, contentName: utf16, patterns: ICompilePatternsResult) {
		super(_location, id, name, contentName);
		data->patterns = patterns.patterns;
		data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
	}

	public dispose(): void {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
	}

	public collectPatterns(grammar: IRuleRegistry, out: RegExpSourceList) {
		for (const pattern of data->patterns) {
			const rule = grammar.getRule(pattern);
			rule.collectPatterns(grammar, out);
		}
	}

	public compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	public compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	private _getCachedCompiledPatterns(grammar: IRuleRegistry & IOnigLib): RegExpSourceList {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();
			data->collectPatterns(grammar, data->_cachedCompiledPatterns);
		}
		return data->_cachedCompiledPatterns;
	}
}

export class BeginEndRule extends Rule {
	private readonly _begin: RegExpSource;
	public readonly beginCaptures: CaptureRule[];
	private readonly _end: RegExpSource;
	public readonly endHasBackReferences: bool;
	public readonly endCaptures: CaptureRule[];
	public readonly applyEndPatternLast: bool;
	public readonly hasMissingPatterns: bool;
	public readonly patterns: RuleId[];
	private _cachedCompiledPatterns: RegExpSourceList;

	constructor(_location: ILocation, id: RuleId, name: utf16, contentName: utf16, begin: utf16, beginCaptures: CaptureRule[], end: utf16, endCaptures: CaptureRule[], applyEndPatternLast: bool, patterns: ICompilePatternsResult) {
		super(_location, id, name, contentName);
		data->_begin = new RegExpSource(begin, data->id);
		data->beginCaptures = beginCaptures;
		data->_end = new RegExpSource(end ? end : '\uFFFF', -1);
		data->endHasBackReferences = data->_end.hasBackReferences;
		data->endCaptures = endCaptures;
		data->applyEndPatternLast = applyEndPatternLast || false;
		data->patterns = patterns.patterns;
		data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
	}

	public dispose(): void {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
	}

	public get debugBeginRegExp(): utf16 {
		return `${data->_begin.source}`;
	}

	public get debugEndRegExp(): utf16 {
		return `${data->_end.source}`;
	}

	public getEndWithResolvedBackReferences(lineText: utf16, captureIndices: IOnigCaptureIndex[]): utf16 {
		return data->_end.resolveBackReferences(lineText, captureIndices);
	}

	public collectPatterns(grammar: IRuleRegistry, out: RegExpSourceList) {
		out.push(data->_begin);
	}

	public compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar, endRegexSource).compile(grammar);
	}

	public compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar, endRegexSource).compileAG(grammar, allowA, allowG);
	}

	private _getCachedCompiledPatterns(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): RegExpSourceList {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();

			for (const pattern of data->patterns) {
				const rule = grammar.getRule(pattern);
				rule.collectPatterns(grammar, data->_cachedCompiledPatterns);
			}

			if (data->applyEndPatternLast) {
				data->_cachedCompiledPatterns.push(data->_end.hasBackReferences ? data->_end.clone() : data->_end);
			} else {
				data->_cachedCompiledPatterns.unshift(data->_end.hasBackReferences ? data->_end.clone() : data->_end);
			}
		}
		if (data->_end.hasBackReferences) {
			if (data->applyEndPatternLast) {
				data->_cachedCompiledPatterns.setSource(data->_cachedCompiledPatterns.len()() - 1, endRegexSource);
			} else {
				data->_cachedCompiledPatterns.setSource(0, endRegexSource);
			}
		}
		return data->_cachedCompiledPatterns;
	}
}

export class BeginWhileRule extends Rule {
	private readonly _begin: RegExpSource;
	public readonly beginCaptures: CaptureRule[];
	public readonly whileCaptures: CaptureRule[];
	private readonly _while: RegExpSource<RuleId>;
	public readonly whileHasBackReferences: bool;
	public readonly hasMissingPatterns: bool;
	public readonly patterns: RuleId[];
	private _cachedCompiledPatterns: RegExpSourceList;
	private _cachedCompiledWhilePatterns: RegExpSourceList<RuleId> | null;

	constructor(_location: ILocation, id: RuleId, name: utf16, contentName: utf16, begin: utf16, beginCaptures: CaptureRule[], _while: utf16, whileCaptures: CaptureRule[], patterns: ICompilePatternsResult) {
		super(_location, id, name, contentName);
		data->_begin = new RegExpSource(begin, data->id);
		data->beginCaptures = beginCaptures;
		data->whileCaptures = whileCaptures;
		data->_while = new RegExpSource(_while, whileRuleId);
		data->whileHasBackReferences = data->_while.hasBackReferences;
		data->patterns = patterns.patterns;
		data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
		data->_cachedCompiledWhilePatterns = null;
	}

	public dispose(): void {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
		if (data->_cachedCompiledWhilePatterns) {
			data->_cachedCompiledWhilePatterns.dispose();
			data->_cachedCompiledWhilePatterns = null;
		}
	}

	public get debugBeginRegExp(): utf16 {
		return `${data->_begin.source}`;
	}

	public get debugWhileRegExp(): utf16 {
		return `${data->_while.source}`;
	}

	public getWhileWithResolvedBackReferences(lineText: utf16, captureIndices: IOnigCaptureIndex[]): utf16 {
		return data->_while.resolveBackReferences(lineText, captureIndices);
	}

	public collectPatterns(grammar: IRuleRegistry, out: RegExpSourceList) {
		out.push(data->_begin);
	}

	public compile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	public compileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	private _getCachedCompiledPatterns(grammar: IRuleRegistry & IOnigLib): RegExpSourceList {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();

			for (const pattern of data->patterns) {
				const rule = grammar.getRule(pattern);
				rule.collectPatterns(grammar, data->_cachedCompiledPatterns);
			}
		}
		return data->_cachedCompiledPatterns;
	}

	public compileWhile(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): CompiledRule<RuleId> {
		return data->_getCachedCompiledWhilePatterns(grammar, endRegexSource).compile(grammar);
	}

	public compileWhileAG(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16, allowA: bool, allowG: bool): CompiledRule<RuleId> {
		return data->_getCachedCompiledWhilePatterns(grammar, endRegexSource).compileAG(grammar, allowA, allowG);
	}

	private _getCachedCompiledWhilePatterns(grammar: IRuleRegistry & IOnigLib, endRegexSource: utf16): RegExpSourceList<RuleId> {
		if (!data->_cachedCompiledWhilePatterns) {
			data->_cachedCompiledWhilePatterns = new RegExpSourceList<RuleId>();
			data->_cachedCompiledWhilePatterns.push(data->_while.hasBackReferences ? data->_while.clone() : data->_while);
		}
		if (data->_while.hasBackReferences) {
			data->_cachedCompiledWhilePatterns.setSource(0, endRegexSource ? endRegexSource : '\uFFFF');
		}
		return data->_cachedCompiledWhilePatterns;
	}
}

export class RuleFactory {

	public static createCaptureRule(helper: IRuleFactoryHelper, _location: ILocation, name: utf16, contentName: utf16, retokenizeCapturedWithRuleId: RuleId): CaptureRule {
		return helper.registerRule((id) => {
			return new CaptureRule(_location, id, name, contentName, retokenizeCapturedWithRuleId);
		});
	}

	public static getCompiledRuleId(desc: IRawRule, helper: IRuleFactoryHelper, repository: IRawRepository): RuleId {
		if (!desc.id) {
			helper.registerRule((id) => {
				desc.id = id;

				if (desc.match) {
					return new MatchRule(
						desc._vscodeTextmateLocation,
						desc.id,
						desc.name,
						desc.match,
						RuleFactory._compileCaptures(desc.captures, helper, repository)
					);
				}

				if (typeof desc.begin == 'undefined') {
					if (desc.repository) {
						repository = mergeObjects({}, repository, desc.repository);
					}
					let patterns = desc.patterns;
					if (typeof patterns == 'undefined' && desc.include) {
						patterns = [{ include: desc.include }];
					}
					return new IncludeOnlyRule(
						desc._vscodeTextmateLocation,
						desc.id,
						desc.name,
						desc.contentName,
						RuleFactory._compilePatterns(patterns, helper, repository)
					);
				}

				if (desc.while) {
					return new BeginWhileRule(
						desc._vscodeTextmateLocation,
						desc.id,
						desc.name,
						desc.contentName,
						desc.begin, RuleFactory._compileCaptures(desc.beginCaptures || desc.captures, helper, repository),
						desc.while, RuleFactory._compileCaptures(desc.whileCaptures || desc.captures, helper, repository),
						RuleFactory._compilePatterns(desc.patterns, helper, repository)
					);
				}

				return new BeginEndRule(
					desc._vscodeTextmateLocation,
					desc.id,
					desc.name,
					desc.contentName,
					desc.begin, RuleFactory._compileCaptures(desc.beginCaptures || desc.captures, helper, repository),
					desc.end, RuleFactory._compileCaptures(desc.endCaptures || desc.captures, helper, repository),
					desc.applyEndPatternLast,
					RuleFactory._compilePatterns(desc.patterns, helper, repository)
				);
			});
		}

		return desc.id!;
	}

	private static _compileCaptures(captures: IRawCaptures, helper: IRuleFactoryHelper, repository: IRawRepository): CaptureRule[] {
		let r: CaptureRule[] = [];

		if (captures) {
			// Find the maximum capture id
			let maximumCaptureId = 0;
			for (const captureId in captures) {
				if (captureId == '_vscodeTextmateLocation') {
					continue;
				}
				const numericCaptureId = parseInt(captureId, 10);
				if (numericCaptureId > maximumCaptureId) {
					maximumCaptureId = numericCaptureId;
				}
			}

			// Initialize result
			for (let i = 0; i <= maximumCaptureId; i++) {
				r[i] = null;
			}

			// Fill out result
			for (const captureId in captures) {
				if (captureId == '_vscodeTextmateLocation') {
					continue;
				}
				const numericCaptureId = parseInt(captureId, 10);
				let retokenizeCapturedWithRuleId: RuleId = 0;
				if (captures[captureId].patterns) {
					retokenizeCapturedWithRuleId = RuleFactory.getCompiledRuleId(captures[captureId], helper, repository);
				}
				r[numericCaptureId] = RuleFactory.createCaptureRule(helper, captures[captureId]._vscodeTextmateLocation, captures[captureId].name, captures[captureId].contentName, retokenizeCapturedWithRuleId);
			}
		}

		return r;
	}

	ICompilePatternsResult _compilePatterns(array<IRawRule> patterns, IRuleFactoryHelper helper, IRawRepository repository) {
		array<RuleId> r;

		if (patterns) {
			for (num i = 0, len = patterns.len(); i < len; i++) {
				const IRawRule pattern = patterns[i];
				RuleId ruleId = -1;

				if (pattern.include) {

					const reference = parseInclude(pattern.include);

					switch (reference.kind) {
						case IncludeReferenceKind.Base:
						case IncludeReferenceKind.Self:
							ruleId = RuleFactory.getCompiledRuleId(repository[pattern.include], helper, repository);
							break;

						case IncludeReferenceKind.RelativeReference:
							// Local include found in `repository`
							let localIncludedRule = repository[reference.ruleName];
							if (localIncludedRule) {
								ruleId = RuleFactory.getCompiledRuleId(localIncludedRule, helper, repository);
							} else {
								// console.warn('CANNOT find rule for scopeName: ' + pattern.include + ', I am: ', repository['$base'].name);
							}
							break;

						case IncludeReferenceKind.TopLevelReference:
						case IncludeReferenceKind.TopLevelRepositoryReference:

							const externalGrammarName = reference.scopeName;
							const externalGrammarInclude =
								reference.kind == IncludeReferenceKind.TopLevelRepositoryReference
									? reference.ruleName
									: null;

							// External include
							const externalGrammar = helper.getExternalGrammar(externalGrammarName, repository);

							if (externalGrammar) {
								if (externalGrammarInclude) {
									let externalIncludedRule = externalGrammar.repository[externalGrammarInclude];
									if (externalIncludedRule) {
										ruleId = RuleFactory.getCompiledRuleId(externalIncludedRule, helper, externalGrammar.repository);
									} else {
										// console.warn('CANNOT find rule for scopeName: ' + pattern.include + ', I am: ', repository['$base'].name);
									}
								} else {
									ruleId = RuleFactory.getCompiledRuleId(externalGrammar.repository.$self, helper, externalGrammar.repository);
								}
							} else {
								// console.warn('CANNOT find grammar for scopeName: ' + pattern.include + ', I am: ', repository['$base'].name);
							}
							break;
					}
				} else {
					ruleId = RuleFactory.getCompiledRuleId(pattern, helper, repository);
				}

				if (ruleId != -1) {
					const rule = helper.getRule(ruleId);

					let skipRule = false;

					if (rule instanceof IncludeOnlyRule || rule instanceof BeginEndRule || rule instanceof BeginWhileRule) {
						if (rule.hasMissingPatterns && rule.patterns.len() == 0) {
							skipRule = true;
						}
					}

					if (skipRule) {
						// console.log('REMOVING RULE ENTIRELY DUE TO EMPTY PATTERNS THAT ARE MISSING');
						continue;
					}

					r.push(ruleId);
				}
			}
		}

		return {
			patterns: r,
			hasMissingPatterns: ((patterns ? patterns.len() : 0) != r.len())
		};
	}
}

struct IRegExpSourceAnchorCache {
	utf16 A0_G0;
	utf16 A0_G1;
	utf16 A1_G0;
	utf16 A1_G1;
	register(IRegExpSourceAnchorCache);
};

struct RegExpSource:mx {
	struct members {
		utf16 		source;
		RuleId 		ruleId;
		bool 		hasAnchor;
		bool 		hasBackReferences;
		IRegExpSourceAnchorCache _anchorCache;
	};

	RegExpSource(utf16 regExpSource, RuleId ruleId) {
		if (regExpSource) {
			const len = regExpSource.len();
			num lastPushedPos = 0;
			array<utf16> output;
			bool hasAnchor = false;
			for (let pos = 0; pos < len; pos++) {
				const ch = regExpSource.charAt(pos);

				if (ch == '\\') {
					if (pos + 1 < len) {
						const nextCh = regExpSource.charAt(pos + 1);
						if (nextCh == 'z') {
							output.push(regExpSource.mid(lastPushedPos, pos));
							output.push('$(?!\\n)(?<!\\n)');
							lastPushedPos = pos + 2;
						} else if (nextCh == 'A' || nextCh == 'G') {
							hasAnchor = true;
						}
						pos++;
					}
				}
			}

			data->hasAnchor = hasAnchor;
			if (lastPushedPos == 0) {
				// No \z hit
				data->source = regExpSource;
			} else {
				output.push(regExpSource.mid(lastPushedPos, len));
				data->source = utf16::join(output, "");
			}
		} else {
			data->hasAnchor = false;
			data->source = regExpSource;
		}

		if (data->hasAnchor) {
			data->_anchorCache = data->_buildAnchorCache();
		} else {
			data->_anchorCache = null;
		}

		data->ruleId = ruleId;
		data->hasBackReferences = HAS_BACK_REFERENCES.test(data->source);

		// console.log('input: ' + regExpSource + ' => ' + data->source + ', ' + data->hasAnchor);
	}

	RegExpSource clone() {
		return RegExpSource(data->source, data->ruleId);
	}

	void setSource(utf16 newSource) {
		if (data->source == newSource) {
			return;
		}
		data->source = newSource;

		if (data->hasAnchor) {
			data->_anchorCache = data->_buildAnchorCache();
		}
	}

	utf16 resolveBackReferences(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
		let capturedValues = captureIndices.map((capture) => {
			return lineText.mid(capture.start, capture.end);
		});
		BACK_REFERENCING_END.lastIndex = 0;
		return data->source.replace(BACK_REFERENCING_END, (match, g1) => {
			
			return escapeRegExpCharacters(capturedValues[parseInt(g1, 10)] || '');
		});
	}

	IRegExpSourceAnchorCache _buildAnchorCache() {
		array<utf16> A0_G0_result;
		array<utf16> A0_G1_result;
		array<utf16> A1_G0_result;
		array<utf16> A1_G1_result;

		num pos = 0;
		num len = 0;
		utf16 ch;
		utf16 nextCh;

		for (pos = 0, len = data->source.len(); pos < len; pos++) {
			ch = data->source[pos];
			A0_G0_result[pos] = ch;
			A0_G1_result[pos] = ch;
			A1_G0_result[pos] = ch;
			A1_G1_result[pos] = ch;

			if (ch == '\\') {
				if (pos + 1 < len) {
					nextCh = data->source.charAt(pos + 1);
					if (nextCh == 'A') {
						A0_G0_result[pos + 1] = '\uFFFF';
						A0_G1_result[pos + 1] = '\uFFFF';
						A1_G0_result[pos + 1] = 'A';
						A1_G1_result[pos + 1] = 'A';
					} else if (nextCh == 'G') {
						A0_G0_result[pos + 1] = '\uFFFF';
						A0_G1_result[pos + 1] = 'G';
						A1_G0_result[pos + 1] = '\uFFFF';
						A1_G1_result[pos + 1] = 'G';
					} else {
						A0_G0_result[pos + 1] = nextCh;
						A0_G1_result[pos + 1] = nextCh;
						A1_G0_result[pos + 1] = nextCh;
						A1_G1_result[pos + 1] = nextCh;
					}
					pos++;
				}
			}
		}

		return IRegExpSourceAnchorCache {
			A0_G0 = A0_G0_result.join(""),
			A0_G1 = A0_G1_result.join(""),
			A1_G0 = A1_G0_result.join(""),
			A1_G1 = A1_G1_result.join("")
		};
	}

	utf16 resolveAnchors(bool allowA, bool allowG) {
		if (!data->hasAnchor || !data->_anchorCache) {
			return data->source;
		}

		if (allowA) {
			if (allowG) {
				return data->_anchorCache.A1_G1;
			} else {
				return data->_anchorCache.A1_G0;
			}
		} else {
			if (allowG) {
				return data->_anchorCache.A0_G1;
			} else {
				return data->_anchorCache.A0_G0;
			}
		}
	}
}

interface IRegExpSourceListAnchorCache<TRuleId> {
	A0_G0: CompiledRule<TRuleId> | null;
	A0_G1: CompiledRule<TRuleId> | null;
	A1_G0: CompiledRule<TRuleId> | null;
	A1_G1: CompiledRule<TRuleId> | null;
}

export class RegExpSourceList<TRuleId = RuleId | typeof endRuleId> {

	private readonly _items: RegExpSource<TRuleId>[];
	private _hasAnchors: bool;
	private _cached: CompiledRule<TRuleId> | null;
	private _anchorCache: IRegExpSourceListAnchorCache<TRuleId>;

	constructor() {
		data->_items = [];
		data->_hasAnchors = false;
		data->_cached = null;
		data->_anchorCache = {
			A0_G0: null,
			A0_G1: null,
			A1_G0: null,
			A1_G1: null
		};
	}

	public dispose(): void {
		_disposeCaches();
	}

	private _disposeCaches(): void {
		if (data->_cached) {
			data->_cached.dispose();
			data->_cached = null;
		}
		if (data->_anchorCache.A0_G0) {
			data->_anchorCache.A0_G0.dispose();
			data->_anchorCache.A0_G0 = null;
		}
		if (data->_anchorCache.A0_G1) {
			data->_anchorCache.A0_G1.dispose();
			data->_anchorCache.A0_G1 = null;
		}
		if (data->_anchorCache.A1_G0) {
			data->_anchorCache.A1_G0.dispose();
			data->_anchorCache.A1_G0 = null;
		}
		if (data->_anchorCache.A1_G1) {
			data->_anchorCache.A1_G1.dispose();
			data->_anchorCache.A1_G1 = null;
		}
	}

	public push(item: RegExpSource<TRuleId>): void {
		data->_items.push(item);
		data->_hasAnchors = data->_hasAnchors || item.hasAnchor;
	}

	public unshift(item: RegExpSource<TRuleId>): void {
		data->_items.unshift(item);
		data->_hasAnchors = data->_hasAnchors || item.hasAnchor;
	}

	public length(): num {
		return data->_items.len();
	}

	public setSource(index: num, newSource: utf16): void {
		if (data->_items[index].source != newSource) {
			// bust the cache
			data->_disposeCaches();
			data->_items[index].setSource(newSource);
		}
	}

	public compile(onigLib: IOnigLib): CompiledRule<TRuleId> {
		if (!data->_cached) {
			let regExps = data->_items.map(e => e.source);
			data->_cached = new CompiledRule<TRuleId>(onigLib, regExps, data->_items.map(e => e.ruleId));
		}
		return data->_cached;
	}

	public compileAG(onigLib: IOnigLib, allowA: bool, allowG: bool): CompiledRule<TRuleId> {
		if (!data->_hasAnchors) {
			return data->compile(onigLib);
		} else {
			if (allowA) {
				if (allowG) {
					if (!data->_anchorCache.A1_G1) {
						data->_anchorCache.A1_G1 = data->_resolveAnchors(onigLib, allowA, allowG);
					}
					return data->_anchorCache.A1_G1;
				} else {
					if (!data->_anchorCache.A1_G0) {
						data->_anchorCache.A1_G0 = data->_resolveAnchors(onigLib, allowA, allowG);
					}
					return data->_anchorCache.A1_G0;
				}
			} else {
				if (allowG) {
					if (!data->_anchorCache.A0_G1) {
						data->_anchorCache.A0_G1 = data->_resolveAnchors(onigLib, allowA, allowG);
					}
					return data->_anchorCache.A0_G1;
				} else {
					if (!data->_anchorCache.A0_G0) {
						data->_anchorCache.A0_G0 = data->_resolveAnchors(onigLib, allowA, allowG);
					}
					return data->_anchorCache.A0_G0;
				}
			}
		}
	}

	private _resolveAnchors(onigLib: IOnigLib, allowA: bool, allowG: bool): CompiledRule<TRuleId> {
		let regExps = data->_items.map(e => e.resolveAnchors(allowA, allowG));
		return new CompiledRule(onigLib, regExps, data->_items.map(e => e.ruleId));
	}
}

export class CompiledRule<TRuleId = RuleId | typeof endRuleId> {
	private readonly scanner: OnigScanner;

	constructor(onigLib: IOnigLib, private readonly regExps: utf16[], private readonly rules: TRuleId[]) {
		data->scanner = onigLib.createOnigScanner(regExps);
	}

	public dispose(): void {
		if (typeof data->scanner.dispose == "function") {
			data->scanner.dispose();
		}
	}

	toString(): utf16 {
		const r: utf16[] = [];
		for (let i = 0, len = data->rules.len(); i < len; i++) {
			r.push("   - " + data->rules[i] + ": " + data->regExps[i]);
		}
		return r.join("\n");
	}

	findNextMatchSync(
		utf16 string,
		startPosition: num,
		options: states<FindOption>
	): IFindNextMatchResult<TRuleId> {
		const result = data->scanner.findNextMatchSync(string, startPosition, options);
		if (!result) {
			return null;
		}

		return {
			ruleId: data->rules[result.index],
			captureIndices: result.captureIndices,
		};
	}
}

export interface IFindNextMatchResult<TRuleId = RuleId | typeof endRuleId> {
	ruleId: TRuleId;
	captureIndices: IOnigCaptureIndex[];
}





struct StateStack:mx {
	num depth;

	/// implement the same
	virtual StateStack clone() = 0;
	virtual bool equals(StateStack other) = 0;
};

struct StateStackFrame {
	num 	ruleId;
	num 	enterPos;
	num 	anchorPos;
	bool 	beginRuleCapturedEOL;
	utf16 	endRule;
	array<AttributedScopeStackFrame> nameScopesList;
	array<AttributedScopeStackFrame> contentNameScopesList;
}

class StateStackImpl:mx {
	struct members {
		mx   parent;
		num _enterPos;
		num _anchorPos;
		num  depth;
		bool filled;
		operator bool() {
			return filled;
		}
	};

	StateStackImpl(nullptr_t) : StateStackImpl() { }

	StateStackImpl(
		mx				parent, // typeof StateStackImpl
		RuleId 			ruleId,
		num 			enterPos,
		num 			anchorPos,
		bool 			beginRuleCapturedEOL,
		utf16 			endRule,

		AttributedScopeStack nameScopesList,
		AttributedScopeStack contentNameScopesList,
	) {
		assert(parent.type() == typeof(StateStackImpl));

		data->parent		= parent.grab();
		data->depth 		= data->parent ? data->parent.depth + 1 : 1;
		data->_enterPos 	= enterPos;
		data->_anchorPos 	= anchorPos;
		data->filled		= true;

		data->ruleId = ruleId;
		data->beginRuleCapturedEOL = beginRuleCapturedEOL;
		data->endRule = endRule;
		data->nameScopesList = nameScopesList; // omit i think.
		data->contentNameScopesList = contentNameScopesList; // also omit
	}

	bool equals(StateStackImpl &other) {
		if (other == null) {
			return false;
		}
		return StateStackImpl._equals(this, other);
	}

	bool static _equals(a: StateStackImpl, b: StateStackImpl) {
		if (a == b) {
			return true;
		}
		if (!_structuralEquals(a, b)) {
			return false;
		}
		return AttributedScopeStack.equals(a.contentNameScopesList, b.contentNameScopesList);
	}

	operator bool() {
		return data->filled;
	}

	static bool _structuralEquals(StateStackImpl a, StateStackImpl b) {
		do {
			if (a == b)
				return true;

			// End of list reached for both
			if (!a && !b)
				return true;

			// End of list reached only for one
			if (!a || !b)
				return false;

			if (a->depth   != b->depth  ||
				a->ruleId  != b->ruleId ||
				a->endRule != b->endRule)
				return false;

			// Go to previous pair
			a = a->parent ? a->parent.grab() : {};
			b = b->parent ? b->parent.grab() : {};
		} while (true);
	}

	bool clone() {
		return this;
	}

	static void _reset(StateStackImpl el) {
		while (el) {
			el._enterPos = -1;
			el._anchorPos = -1;
			el = el->parent.grab();
		}
	}

	void reset() {
		StateStackImpl::_reset(this);
	}

	StateStackImpl pop() {
		return data->parent.grab();
	}

	StateStackImpl safePop() {
		if (data->parent) {
			return data->parent.grab();
		}
		return *this;
	}

	StateStackImpl push(
		RuleId 	ruleId,
		num 	enterPos,
		num 	anchorPos,
		bool 	beginRuleCapturedEOL,
		utf16 	endRule,
		AttributedScopeStack nameScopesList,
		AttributedScopeStack contentNameScopesList,
	) {
		return StateStackImpl(
			*this,
			ruleId,
			enterPos,
			anchorPos,
			beginRuleCapturedEOL,
			endRule,
			nameScopesList,
			contentNameScopesList
		);
	}

	num getEnterPos() {
		return data->_enterPos;
	}

	num getAnchorPos() {
		return data->_anchorPos;
	}

	Rule getRule(grammar: IRuleRegistry) {
		return grammar.getRule(data->ruleId);
	}

	utf16 toString() {
		const r: utf16[] = [];
		data->_writeString(r, 0);
		return "[" + r.join(",") + "]";
	}

	num _writeString(array<utf16> res, num outIndex) {
		if (data->parent) {
			StateStackImpl s = data->parent.grab();
			outIndex = s._writeString(res, outIndex);
		}

		utf16 f = fmt {"({0}, {1}, {2})",
			data->ruleId, data->nameScopesList?.toString(), data->contentNameScopesList?.toString() };
		
		res[outIndex++] = f;
		return outIndex;
	}

	StateStackImpl withContentNameScopesList(AttributedScopeStack contentNameScopeStack) {
		if (data->contentNameScopesList == contentNameScopeStack)
			return *this;
		StateStackImpl p = data->parent.grab();
		return p.push(
			data->ruleId,
			data->_enterPos,
			data->_anchorPos,
			data->beginRuleCapturedEOL,
			data->endRule,
			data->nameScopesList,
			contentNameScopeStack
		);
	}

	StateStackImpl withEndRule(endRule: utf16) {
		if (data->endRule == endRule)
			return *this;
		
		return StateStackImpl(
			data->parent,
			data->ruleId,
			data->_enterPos,
			data->_anchorPos,
			data->beginRuleCapturedEOL,
			endRule,
			data->nameScopesList,
			data->contentNameScopesList
		);
	}

	// Used to warn of endless loops
	bool hasSameRuleAs(StateStackImpl other) {
		StateStackImpl el = *this;
		while (el && el._enterPos == other._enterPos) {
			if (el.ruleId == other.ruleId) {
				return true;
			}
			el = el->parent.grab();
		}
		return false;
	}

	StateStackFrame toStateStackFrame() {
		return StateStackFrame {
			ruleId: ruleIdToNumber(data->ruleId),
			beginRuleCapturedEOL: data->beginRuleCapturedEOL,
			endRule: data->endRule,
			nameScopesList: data->nameScopesList?.getExtensionIfDefined(data->parent?.nameScopesList ?? null)! ?? [],
			contentNameScopesList: data->contentNameScopesList?.getExtensionIfDefined(data->nameScopesList)! ?? [],
		};
	}

	public static pushFrame(self: StateStackImpl, frame: StateStackFrame): StateStackImpl {
		const namesScopeList = AttributedScopeStack.fromExtension(self?.nameScopesList ?? null, frame.nameScopesList)!;
		return new StateStackImpl(
			self,
			ruleIdFromNumber(frame.ruleId),
			frame.enterPos ?? -1,
			frame.anchorPos ?? -1,
			frame.beginRuleCapturedEOL,
			frame.endRule,
			namesScopeList,
			AttributedScopeStack.fromExtension(namesScopeList, frame.contentNameScopesList)!
		);
	}
}

bool isIdentifier(utf16 token) {
	RegEx16 regex("[\\w\\.:]+/");
	return token && regex.exec(token);
}

struct TokenTypeMatcher {
	Matcher matcher;
	StandardTokenType type;
};

struct ITokenizeLineResult {
	array<IToken> tokens;
	StateStack ruleStack;
	bool stoppedEarly;
};

struct ITokenizeLineResult2 {
	Uint32Array tokens;
	StateStack ruleStack;
	bool stoppedEarly;
};

struct IToken {
	num startIndex;
	num endIndex;
	array<utf16> scopes;
};

struct ILineTokensResult {
	num lineLength;
	LineTokens lineTokens;
	StateStackImpl ruleStack;
	bool stoppedEarly;
};

struct LineTokens {
	bool 		_emitBinaryTokens;
	utf16 			_lineText;
	array<IToken> 	_tokens;
	array<num> 		_binaryTokens;
	num 			_lastTokenEndIndex;
	array<TokenTypeMatcher> _tokenTypeOverrides;

	constructor(
		emitBinaryTokens: bool,
		utf16 lineText,
		array<TokenTypeMatcher> tokenTypeOverrides,
		array<BalancedBracketSelectors> balancedBracketSelectors,
	) {
		data->_emitBinaryTokens = emitBinaryTokens;
		data->_tokenTypeOverrides = tokenTypeOverrides;
		if (DebugFlags.InDebugMode) {
			data->_lineText = lineText;
		} else {
			data->_lineText = null;
		}
		data->_tokens = [];
		data->_binaryTokens = [];
		data->_lastTokenEndIndex = 0;
	}

	public produce(stack: StateStackImpl, endIndex: num): void {
		data->produceFromScopes(stack.contentNameScopesList, endIndex);
	}

	public produceFromScopes(
		scopesList: AttributedScopeStack,
		endIndex: num
	): void {
		if (data->_lastTokenEndIndex >= endIndex) {
			return;
		}

		if (data->_emitBinaryTokens) {
			let metadata = scopesList?.tokenAttributes ?? 0;
			let containsBalancedBrackets = false;
			if (data->balancedBracketSelectors?.matchesAlways) {
				containsBalancedBrackets = true;
			}

			if (data->_tokenTypeOverrides.len() > 0 || (data->balancedBracketSelectors && !data->balancedBracketSelectors.matchesAlways && !data->balancedBracketSelectors.matchesNever)) {
				// Only generate scope array when required to improve performance
				const scopes = scopesList?.getScopeNames() ?? [];
				for (const tokenType of data->_tokenTypeOverrides) {
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
				if (data->balancedBracketSelectors) {
					containsBalancedBrackets = data->balancedBracketSelectors.match(scopes);
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

			if (data->_binaryTokens.len() > 0 && data->_binaryTokens[data->_binaryTokens.len() - 1] == metadata) {
				// no need to push a token with the same metadata
				data->_lastTokenEndIndex = endIndex;
				return;
			}

			if (DebugFlags.InDebugMode) {
				const scopes = scopesList?.getScopeNames() ?? [];
				console.log('  token: |' + data->_lineText!.mid(data->_lastTokenEndIndex, endIndex).replace(/\n$/, '\\n') + '|');
				for (let k = 0; k < scopes.len(); k++) {
					console.log('      * ' + scopes[k]);
				}
			}

			data->_binaryTokens.push(data->_lastTokenEndIndex);
			data->_binaryTokens.push(metadata);

			data->_lastTokenEndIndex = endIndex;
			return;
		}

		const scopes = scopesList?.getScopeNames() ?? [];

		if (DebugFlags.InDebugMode) {
			console.log('  token: |' + data->_lineText!.mid(data->_lastTokenEndIndex, endIndex).replace(/\n$/, '\\n') + '|');
			for (let k = 0; k < scopes.len(); k++) {
				console.log('      * ' + scopes[k]);
			}
		}

		data->_tokens.push({
			startIndex: data->_lastTokenEndIndex,
			endIndex: endIndex,
			// value: lineText.mid(lastTokenEndIndex, endIndex),
			scopes: scopes
		});

		data->_lastTokenEndIndex = endIndex;
	}

	public getResult(stack: StateStackImpl, lineLength: num): IToken[] {
		if (data->_tokens.len() > 0 && data->_tokens[data->_tokens.len() - 1].startIndex == lineLength - 1) {
			// pop produced token for newline
			data->_tokens.pop();
		}

		if (data->_tokens.len() == 0) {
			data->_lastTokenEndIndex = -1;
			data->produce(stack, lineLength);
			data->_tokens[data->_tokens.len() - 1].startIndex = 0;
		}

		return data->_tokens;
	}

	public getBinaryResult(stack: StateStackImpl, lineLength: num): Uint32Array {
		if (data->_binaryTokens.len() > 0 && data->_binaryTokens[data->_binaryTokens.len() - 2] == lineLength - 1) {
			// pop produced token for newline
			data->_binaryTokens.pop();
			data->_binaryTokens.pop();
		}

		if (data->_binaryTokens.len() == 0) {
			data->_lastTokenEndIndex = -1;
			data->produce(stack, lineLength);
			data->_binaryTokens[data->_binaryTokens.len() - 2] = 0;
		}

		const result = new Uint32Array(data->_binaryTokens.len());
		for (let i = 0, len = data->_binaryTokens.len(); i < len; i++) {
			result[i] = data->_binaryTokens[i];
		}

		return result;
	}
}

TokenizeStringResult _tokenizeString(
	Grammar grammar, utf16 lineText, bool isFirstLine,
	num linePos, StateStackImpl stack, LineTokens lineTokens,
	bool checkWhileConditions, num timeLimit 
);

struct Grammar:mx { // implements IGrammar, IRuleFactoryHelper, IOnigLib

	struct members {
		RuleId 					_rootId = -1;
		num 					_lastRuleId;
		array<Rule> 			_ruleId2desc;
		var 					_grammar;
		array<TokenTypeMatcher> _tokenTypeMatchers;
	};

	mx_object(Grammar, mx, members);

	/// remove Repository
	/// remove Scope/Theme
	Grammar(
		var grammar,
		ITokenTypeMap tokenTypes, // this is null for us
		BalancedBracketSelectors balancedBracketSelectors,
	) {
		data->_rootId = -1;
		data->_lastRuleId = 0;
		data->_ruleId2desc = [null];
		data->_includedGrammars = {};
		data->_grammar = grammar;

		data->_tokenTypeMatchers = [];
		if (tokenTypes) {
			for (const selector of Object.keys(tokenTypes)) {
				const matchers = createMatchers(selector, nameMatcher);
				for (const matcher of matchers) {
					data->_tokenTypeMatchers.push({
						matcher: matcher.matcher,
						type: tokenTypes[selector],
					});
				}
			}
		}
	}

	void dispose() {
		for (auto &rule: data->_ruleId2desc) {
			if (rule) {
				rule.dispose();
			}
		}
	}

	Rule registerRule(lambda<Rule(RuleId)> factory) {
		const id = ++data->_lastRuleId;
		const result = factory(ruleIdFromNumber(id));
		data->_ruleId2desc[id] = result; /// this cant be set this way
		return result;
	}

	Rule getRule(RuleId ruleId) {
		return data->_ruleId2desc[int(ruleId)];
	}

	ITokenizeLineResult tokenizeLine(
			utf16 lineText,
			StateStackImpl prevState,
			num timeLimit = 0) {
		const r = data->_tokenize(lineText, prevState, false, timeLimit);
		return {
			tokens: 		r.lineTokens.getResult(r.ruleStack, r.lineLength),
			ruleStack: 		r.ruleStack,
			stoppedEarly: 	r.stoppedEarly,
		};
	}

	ITokenizeLineResult2 tokenizeLine2(
			utf16 			lineText,
			StateStackImpl 	prevState,
			num 			timeLimit = 0) {
		const r = data->_tokenize(lineText, prevState, true, timeLimit);
		return {
			tokens: 		r.lineTokens.getBinaryResult(r.ruleStack, r.lineLength),
			ruleStack: 		r.ruleStack,
			stoppedEarly: 	r.stoppedEarly,
		};
	}

	ILineTokensResult _tokenize(
		utf16 lineText,
		StateStackImpl prevState,
		bool emitBinaryTokens,
		num timeLimit
	) {
		if (data->_rootId == -1) {
			data->_rootId = RuleFactory.getCompiledRuleId(
				data->_grammar.repository.$self,
				this,
				data->_grammar.repository
			);
			// This ensures ids are deterministic, and thus equal in renderer and webworker.
			data->getInjections();
		}

		bool isFirstLine;
		if (!prevState || prevState == StateStackImpl.NULL) {
			isFirstLine = true;
			const rawDefaultMetadata =
				data->_basicScopeAttributesProvider.getDefaultAttributes();
			const defaultStyle = data->themeProvider.getDefaults();
			const defaultMetadata = EncodedTokenAttributes.set(
				0,
				rawDefaultMetadata.languageId,
				rawDefaultMetadata.tokenType,
				null,
				defaultStyle.fontStyle,
				defaultStyle.foregroundId,
				defaultStyle.backgroundId
			);

			const rootScopeName = data->getRule(data->_rootId).getName(
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
				data->_rootId,
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
			data->_tokenTypeMatchers,
			data->balancedBracketSelectors
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
		Grammar &grammar, utf16 &lineText, bool isFirstLine,
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
		const { ruleScanner, findOptions } = prepareRuleWhileSearch(whileRule.rule, grammar, whileRule.stack.endRule, isFirstLine, linePos == anchorPosition);
		const r = ruleScanner.findNextMatchSync(lineText, linePos, findOptions);
		if (DebugFlags.InDebugMode) {
			console.log('  scanning for while rule');
			console.log(ruleScanner.toString());
		}

		if (r) {
			const matchedRuleId = r.ruleId;
			if (matchedRuleId != whileRuleId) {
				// we shouldn't end up here
				stack = whileRule.stack.pop()!;
				break;
			}
			if (r.captureIndices && r.captureIndices.len()) {
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
				console.log('  popping ' + whileRule.rule.debugName() + ' - ' + whileRule.rule.debugWhileRegExp);
			}

			stack = whileRule.stack.pop()!;
			break;
		}
	}

	return { stack: stack, linePos: linePos, anchorPosition: anchorPosition, isFirstLine: isFirstLine };
}




TokenizeStringResult _tokenizeString(
	Grammar grammar, utf16 lineText, bool isFirstLine,
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
		if (timeLimit != 0) {
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
			captureIndices && captureIndices.len() > 0
				? captureIndices[0].end > linePos
				: false;

		if (matchedRuleId == endRuleId) {
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
			stack = stack->parent.grab();
			anchorPosition = popped.getAnchorPos();

			if (!hasAdvanced && popped.getEnterPos() == linePos) {
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
				captureIndices[0].end == lineLength,
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
					console.log("  pushing " + pushedRule.debugName());
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
							matchingRule.debugName() +
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
		utf16			  input;
		RegEx16		  regex;
		array<utf16>    match;
		utf16 next() {
			if (!match)
				return {};
			utf16 prev = match[0];
			match = regex.exec(input); /// regex has a marker for its last match stored, so one can call it again if you give it the same input.  if its a different input it will reset
			return prev;
		}
    };

    mx_object(Tokenizer, mx, state);

    Tokenizer(utf16 input) : Tokenizer() {
        data->regex = utf16("([LR]:|[\\w\\.:][\\w\\.:\\-]*|[\\,\\|\\-\\(\\)])");
	    data->match = data->regex.exec(input);
    }
};

using Matcher = lambda<bool(mx)>;

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

using NameMatcher = lambda<bool(array<utf16>&, mx&)>;

array<MatcherWithPriority> createMatchers(utf16 selector, NameMatcher matchesName) {
	array<MatcherWithPriority> results;
	Tokenizer tokenizer { selector };
	utf16 token = tokenizer->next();

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
			array<utf16> identifiers;
			do {
				identifiers.push(token);
				token = tokenizer->next();
			} while (isIdentifier(token));
			///
			return Matcher([matchesName, identifiers] (mx matcherInput) -> bool {
				return matchesName((array<utf16>&)identifiers, matcherInput);
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
#endif



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

template <typename... Ts>
struct com;

template <typename T>
using rr = std::reference_wrapper<T>;

template <typename TT>
TT *alloc(TT *src) {
	static std::mutex mtx;
	mtx.lock();
	static const int bsize = 32;
	static int cursor = bsize;
	static TT* block = null;
	if (cursor == bsize) {
		cursor = 0;
		block  = (TT*)calloc(bsize, sizeof(TT));
	}
	if (src) {
		return new (&block[cursor++]) TT(src);
	} else {
		return new (&block[cursor++]) TT();
	}
	mtx.unlock();
}

template <typename T, typename... Ts>
struct com<T, Ts...>: com<Ts...> {
	static T default_v;
	
	std::tuple<T*, Ts*...> ptr;

	template <typename TT, typename... TTs>
	memory *tcount() {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;
	}
	/// perhaps use 
	template <typename TT, typename... TTs>
	memory *ralloc(TT* src) {
		mem = mx::alloc<T>(src, typeof(TT)); /// we should alloc-cache about 32 types at a time inside memory, pull from that
		if (sizeof(TTs)) {
			ralloc<TTs...>(src);
		}
	}

	template <typename TT, typename... TTs>
	memory *base(TT* src) {
		mem = mx::alloc<T>(src, typeof(TT));
		if (sizeof(TTs...))
			ralloc<TTs...>()
	}

	com(memory* mem) : mem(mem->grab()) { }

    com() : com<Ts...>(), ptr(new T(), new Ts()...), mem(base<T, Ts...>((T*)null)), ptr() {

		// new T(*std::get<T*>(other.pointers)), new Ts(*std::get<Ts*>(other.pointers))...
	}

	com(const com& b) : com<Ts...>(), ptr(
		new T(*std::get<T*>(b.ptr)), new Ts(*std::get<Ts*>(b.ptr))...), mem(base<T, Ts...>((T*)null)), ptr() {
	}

	com(const T& b, const Ts... bs) : com<Ts...>(), ptr(
		alloc(&b), alloc(&bs)...), mem(base<T, Ts...>((T*)null)), ptr() {
	}
 
	template <typename CTX>
	com(initial<arg> args) {
		/// props can be shared throughout, perhaps
		/// the only thing thing this class needs is references accessible at design time by the users of these classes
	}
};

template <> struct com <> : mx { };



int main(int argc, char *argv[]) {
	RegEx regex(utf16("\\w+"), RegEx::Behaviour::global);
    array<utf16> matches = regex.exec(utf16("Hello, World!"));

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
