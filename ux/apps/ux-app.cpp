#include <ux/app.hpp>

using namespace ion;

#if 1

import { RegexSource, mergeObjects, basename, escapeRegExpCharacters, OrMask } from './utils';

import { IOnigLib, OnigScanner, IOnigCaptureIndex, FindOption, IOnigMatch, OnigString } from './onigLib';
import { ILocation, IRawGrammar, IRawRepository, IRawRule, IRawCaptures } from './rawGrammar';
import { IncludeReferenceKind, parseInclude } from './grammar/grammarDependencies';

const RegEx16 HAS_BACK_REFERENCES  = RegEx16(R("\\(\d+)"), RegEx::Behaviour::none);
const RegEx16 BACK_REFERENCING_END = RegEx16(R("\\(\d+)"), RegEx::Behaviour::global);

using ScopeName = str;
using ScopePath = str;
using ScopePattern = str;

struct IGrammarRepository {
	virtual IRawGrammar 	 lookup(ScopeName scopeName) = 0;
	virtual array<ScopeName> injections(ScopeName scopeName) = 0;
};

struct IRawThemeFields {
	str fontStyle;
	str foreground;
	str background;
};

struct IRawThemeSetting {
	str name
	mx scope; // can be ScopePattern or ScopePattern[] ! (so string or array of them)
	sp<IRawThemeFields> settings;
};

struct IRawTheme {
	str name;
	array<IRawThemeSetting> settings;
};

struct ScopeStack:mx {
	struct members {
		ScopeStack parent;
		ScopeName scopeName;
	};

	static push(ScopeStack path, array<ScopeName> scopeNames): ScopeStack | null {
		for (const name of scopeNames) {
			path = new ScopeStack(path, name);
		}
		return path;
	}

	//public static from(first: ScopeName, ...segments: ScopeName[]): ScopeStack;

	//public static from(...segments: ScopeName[]): ScopeStack | null;

	//public static from(...segments: ScopeName[]): ScopeStack | null {
	//	let result: ScopeStack | null = null;
	//	for (let i = 0; i < segments.length; i++) {
	//		result = new ScopeStack(result, segments[i]);
	//	}
	//	return result;
	//}

	ScopeStack(
		ScopeStack parent,
		ScopeName scopeName
	) {
		data->parent = parent;
		data->scopeName = scopeName;
	}

	ScopeStack push(ScopeName scopeName) {
		return new ScopeStack(this, scopeName);
	}

	array<ScopeName> getSegments() {
		ScopeStack item = this;
		const array<ScopeName> result;
		while (item) {
			result.push(item.scopeName);
			item = item.parent;
		}
		result.reverse();
		return result;
	}

	public toString() {
		return this.getSegments().join(' ');
	}

	boolean extends(ScopeStack other) {
		if (this === other) {
			return true;
		}
		if (this.parent === null) {
			return false;
		}
		return this.parent.extends(other);
	}

	array<str> getExtensionIfDefined(ScopeStack base) {
		const array<str> result;
		ScopeStack item = this;
		while (item && item !== base) {
			result.push(item.scopeName);
			item = item.parent;
		}
		return item == base ? result.reverse() : undefined;
	}
}

boolean _scopePathMatchesParentScopes(ScopeStack scopePath, array<ScopeName> parentScopes) {
	if (parentScopes === null) {
		return true;
	}

	let index = 0;
	let scopePattern = parentScopes[index];

	while (scopePath) {
		if (_matchesScope(scopePath.scopeName, scopePattern)) {
			index++;
			if (index === parentScopes.length) {
				return true;
			}
			scopePattern = parentScopes[index];
		}
		scopePath = scopePath.parent;
	}

	return false;
}

boolean _matchesScope(ScopeName scopeName, ScopeName scopePattern) {
	return scopePattern === scopeName || (scopeName.startsWith(scopePattern) && scopeName[scopePattern.length] === '.');
}

struct StyleAttributes:mx {
	struct members {
		states<FontStyle> fontStyle;
		num foregroundId;
		num backgroundId;
	};
	mx_object(StyleAttributes, mx, members);

	StyleAttributes(states<FontStyle> fontStyle, num foregroundId, num backgroundId):StyleAttributes() {
		data->fontStyle = fontStyle;
		data->foregroundId = foregroundId;
		data->backgroundId = backgroundId;
	}
};

/**
 * Parse a raw theme into rules.
 */
array<ParsedThemeRule> parseTheme(IRawTheme source) {
	if (!source) {
		return {};
	}
	if (!source.settings || !Array.isArray(source.settings)) {
		return {};
	}
	let settings = source.settings;
	array<ParsedThemeRule> result;
	num resultLen = 0;
	for (let i = 0, len = settings.len(); i < len; i++) {
		auto &entry = settings[i];

		if (!entry.settings) {
			continue;
		}

		let scopes: string[];
		if (typeof entry.scope === 'string') {
			let _scope = entry.scope;

			// remove leading commas
			_scope = _scope.replace(/^[,]+/, '');

			// remove trailing commans
			_scope = _scope.replace(/[,]+$/, '');

			scopes = _scope.split(',');
		} else if (Array.isArray(entry.scope)) {
			scopes = entry.scope;
		} else {
			scopes = [''];
		}

		let fontStyle: OrMask<FontStyle> = FontStyle.NotSet;
		if (typeof entry.settings.fontStyle === 'string') {
			fontStyle = FontStyle.None;

			let segments = entry.settings.fontStyle.split(' ');
			for (let j = 0, lenJ = segments.length; j < lenJ; j++) {
				let segment = segments[j];
				switch (segment) {
					case 'italic':
						fontStyle = fontStyle | FontStyle.Italic;
						break;
					case 'bold':
						fontStyle = fontStyle | FontStyle.Bold;
						break;
					case 'underline':
						fontStyle = fontStyle | FontStyle.Underline;
						break;
					case 'strikethrough':
						fontStyle = fontStyle | FontStyle.Strikethrough;
						break;
				}
			}
		}

		let foreground: string | null = null;
		if (typeof entry.settings.foreground === 'string' && isValidHexColor(entry.settings.foreground)) {
			foreground = entry.settings.foreground;
		}

		let background: string | null = null;
		if (typeof entry.settings.background === 'string' && isValidHexColor(entry.settings.background)) {
			background = entry.settings.background;
		}

		for (let j = 0, lenJ = scopes.length; j < lenJ; j++) {
			let _scope = scopes[j].trim();

			let segments = _scope.split(' ');

			let scope = segments[segments.length - 1];
			let parentScopes: string[] | null = null;
			if (segments.length > 1) {
				parentScopes = segments.slice(0, segments.length - 1);
				parentScopes.reverse();
			}

			result[resultLen++] = new ParsedThemeRule(
				scope,
				parentScopes,
				i,
				fontStyle,
				foreground,
				background
			);
		}
	}

	return result;
}

export class ParsedThemeRule {
	constructor(
		public readonly scope: ScopeName,
		public readonly parentScopes: ScopeName[] | null,
		public readonly index: number,
		public readonly fontStyle: OrMask<FontStyle>,
		public readonly foreground: string | null,
		public readonly background: string | null,
	) {
	}
}

export const enum FontStyle {
	NotSet = -1,
	None = 0,
	Italic = 1,
	Bold = 2,
	Underline = 4,
	Strikethrough = 8
}

export function fontStyleToString(fontStyle: OrMask<FontStyle>) {
	if (fontStyle === FontStyle.NotSet) {
		return 'not set';
	}

	let style = '';
	if (fontStyle & FontStyle.Italic) {
		style += 'italic ';
	}
	if (fontStyle & FontStyle.Bold) {
		style += 'bold ';
	}
	if (fontStyle & FontStyle.Underline) {
		style += 'underline ';
	}
	if (fontStyle & FontStyle.Strikethrough) {
		style += 'strikethrough ';
	}
	if (style === '') {
		style = 'none';
	}
	return style.trim();
}

/**
 * Resolve rules (i.e. inheritance).
 */
function resolveParsedThemeRules(parsedThemeRules: ParsedThemeRule[], _colorMap: string[] | undefined): Theme {

	// Sort rules lexicographically, and then by index if necessary
	parsedThemeRules.sort((a, b) => {
		let r = strcmp(a.scope, b.scope);
		if (r !== 0) {
			return r;
		}
		r = strArrCmp(a.parentScopes, b.parentScopes);
		if (r !== 0) {
			return r;
		}
		return a.index - b.index;
	});

	// Determine defaults
	let defaultFontStyle = FontStyle.None;
	let defaultForeground = '#000000';
	let defaultBackground = '#ffffff';
	while (parsedThemeRules.length >= 1 && parsedThemeRules[0].scope === '') {
		let incomingDefaults = parsedThemeRules.shift()!;
		if (incomingDefaults.fontStyle !== FontStyle.NotSet) {
			defaultFontStyle = incomingDefaults.fontStyle;
		}
		if (incomingDefaults.foreground !== null) {
			defaultForeground = incomingDefaults.foreground;
		}
		if (incomingDefaults.background !== null) {
			defaultBackground = incomingDefaults.background;
		}
	}
	let colorMap = new ColorMap(_colorMap);
	let defaults = new StyleAttributes(defaultFontStyle, colorMap.getId(defaultForeground), colorMap.getId(defaultBackground));

	let root = new ThemeTrieElement(new ThemeTrieElementRule(0, null, FontStyle.NotSet, 0, 0), []);
	for (let i = 0, len = parsedThemeRules.length; i < len; i++) {
		let rule = parsedThemeRules[i];
		root.insert(0, rule.scope, rule.parentScopes, rule.fontStyle, colorMap.getId(rule.foreground), colorMap.getId(rule.background));
	}

	return new Theme(colorMap, defaults, root);
}

export class ColorMap {
	private readonly _isFrozen: boolean;
	private _lastColorId: number;
	private _id2color: string[];
	private _color2id: { [color: string]: number; };

	constructor(_colorMap?: string[]) {
		this._lastColorId = 0;
		this._id2color = [];
		this._color2id = Object.create(null);

		if (Array.isArray(_colorMap)) {
			this._isFrozen = true;
			for (let i = 0, len = _colorMap.length; i < len; i++) {
				this._color2id[_colorMap[i]] = i;
				this._id2color[i] = _colorMap[i];
			}
		} else {
			this._isFrozen = false;
		}
	}

	public getId(color: string | null): number {
		if (color === null) {
			return 0;
		}
		color = color.toUpperCase();
		let value = this._color2id[color];
		if (value) {
			return value;
		}
		if (this._isFrozen) {
			throw new Error(`Missing color in color map - ${color}`);
		}
		value = ++this._lastColorId;
		this._color2id[color] = value;
		this._id2color[value] = color;
		return value;
	}

	array<str> getColorMap() {
		return this._id2color.slice(0);
	}
}

export class ThemeTrieElementRule {

	scopeDepth: number;
	parentScopes: ScopeName[] | null;
	fontStyle: number;
	foreground: number;
	background: number;

	constructor(scopeDepth: number, parentScopes: ScopeName[] | null, fontStyle: number, foreground: number, background: number) {
		this.scopeDepth = scopeDepth;
		this.parentScopes = parentScopes;
		this.fontStyle = fontStyle;
		this.foreground = foreground;
		this.background = background;
	}

	ThemeTrieElementRule clone() {
		return new ThemeTrieElementRule(this.scopeDepth, this.parentScopes, this.fontStyle, this.foreground, this.background);
	}

	public static cloneArr(arr:ThemeTrieElementRule[]): ThemeTrieElementRule[] {
		let r: ThemeTrieElementRule[] = [];
		for (let i = 0, len = arr.length; i < len; i++) {
			r[i] = arr[i].clone();
		}
		return r;
	}

	void acceptOverwrite(scopeDepth: number, fontStyle: number, foreground: number, background: number) {
		if (this.scopeDepth > scopeDepth) {
			console.log('how did this happen?');
		} else {
			this.scopeDepth = scopeDepth;
		}
		// console.log('TODO -> my depth: ' + this.scopeDepth + ', overwriting depth: ' + scopeDepth);
		if (fontStyle !== FontStyle.NotSet) {
			this.fontStyle = fontStyle;
		}
		if (foreground !== 0) {
			this.foreground = foreground;
		}
		if (background !== 0) {
			this.background = background;
		}
	}
}

struct ThemeTrieElement:mx {
	using ITrieChildrenMap = map<ThemeTrieElement>;

	private readonly _rulesWithParentScopes: ThemeTrieElementRule[];

	constructor(
		private readonly _mainRule: ThemeTrieElementRule,
		rulesWithParentScopes: ThemeTrieElementRule[] = [],
		private readonly _children: ITrieChildrenMap = {}
	) {
		this._rulesWithParentScopes = rulesWithParentScopes;
	}

	private static _sortBySpecificity(arr: ThemeTrieElementRule[]): ThemeTrieElementRule[] {
		if (arr.length === 1) {
			return arr;
		}
		arr.sort(this._cmpBySpecificity);
		return arr;
	}

	private static _cmpBySpecificity(a: ThemeTrieElementRule, b: ThemeTrieElementRule): number {
		if (a.scopeDepth === b.scopeDepth) {
			const aParentScopes = a.parentScopes;
			const bParentScopes = b.parentScopes;
			let aParentScopesLen = aParentScopes === null ? 0 : aParentScopes.length;
			let bParentScopesLen = bParentScopes === null ? 0 : bParentScopes.length;
			if (aParentScopesLen === bParentScopesLen) {
				for (let i = 0; i < aParentScopesLen; i++) {
					const aLen = aParentScopes![i].length;
					const bLen = bParentScopes![i].length;
					if (aLen !== bLen) {
						return bLen - aLen;
					}
				}
			}
			return bParentScopesLen - aParentScopesLen;
		}
		return b.scopeDepth - a.scopeDepth;
	}

	array<ThemeTrieElementRule> match(scope: ScopeName) {
		if (scope === '') {
			return ThemeTrieElement._sortBySpecificity((<ThemeTrieElementRule[]>[]).concat(this._mainRule).concat(this._rulesWithParentScopes));
		}

		let dotIndex = scope.indexOf('.');
		let head: string;
		let tail: string;
		if (dotIndex === -1) {
			head = scope;
			tail = '';
		} else {
			head = scope.substring(0, dotIndex);
			tail = scope.substring(dotIndex + 1);
		}

		if (this._children.hasOwnProperty(head)) {
			return this._children[head].match(tail);
		}

		return ThemeTrieElement._sortBySpecificity((<ThemeTrieElementRule[]>[]).concat(this._mainRule).concat(this._rulesWithParentScopes));
	}

	public insert(scopeDepth: number, scope: ScopeName, parentScopes: ScopeName[] | null, fontStyle: number, foreground: number, background: number): void {
		if (scope === '') {
			this._doInsertHere(scopeDepth, parentScopes, fontStyle, foreground, background);
			return;
		}

		let dotIndex = scope.indexOf('.');
		let head: string;
		let tail: string;
		if (dotIndex === -1) {
			head = scope;
			tail = '';
		} else {
			head = scope.substring(0, dotIndex);
			tail = scope.substring(dotIndex + 1);
		}

		let child: ThemeTrieElement;
		if (this._children.hasOwnProperty(head)) {
			child = this._children[head];
		} else {
			child = new ThemeTrieElement(this._mainRule.clone(), ThemeTrieElementRule.cloneArr(this._rulesWithParentScopes));
			this._children[head] = child;
		}

		child.insert(scopeDepth + 1, tail, parentScopes, fontStyle, foreground, background);
	}

	private _doInsertHere(scopeDepth: number, parentScopes: ScopeName[] | null, fontStyle: number, foreground: number, background: number): void {

		if (parentScopes === null) {
			// Merge into the main rule
			this._mainRule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
			return;
		}

		// Try to merge into existing rule
		for (let i = 0, len = this._rulesWithParentScopes.length; i < len; i++) {
			let rule = this._rulesWithParentScopes[i];

			if (strArrCmp(rule.parentScopes, parentScopes) === 0) {
				// bingo! => we get to merge this into an existing one
				rule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
				return;
			}
		}

		// Must add a new rule

		// Inherit from main rule
		if (fontStyle === FontStyle.NotSet) {
			fontStyle = this._mainRule.fontStyle;
		}
		if (foreground === 0) {
			foreground = this._mainRule.foreground;
		}
		if (background === 0) {
			background = this._mainRule.background;
		}

		this._rulesWithParentScopes.push(new ThemeTrieElementRule(scopeDepth, parentScopes, fontStyle, foreground, background));
	}
};

using ITrieChildrenMap = ThemeTrieElement::map<ThemeTrieElement>;

struct Theme:mx {
	struct members {
		ColorMap _colorMap;
		StyleAttributes _defaults;
		ThemeTrieElement _root;
	};
	mx_object(Theme, mx, members);

	public static createFromRawTheme(
		source: IRawTheme | undefined,
		colorMap?: string[]
	): Theme {
		return this.createFromParsedTheme(parseTheme(source), colorMap);
	}

	public static createFromParsedTheme(
		source: ParsedThemeRule[],
		colorMap?: string[]
	): Theme {
		return resolveParsedThemeRules(source, colorMap);
	}

	private readonly _cachedMatchRoot = new CachedFn<ScopeName, ThemeTrieElementRule[]>(
		(scopeName) => this._root.match(scopeName)
	);

	constructor(
		private readonly _colorMap: ColorMap,
		private readonly _defaults: StyleAttributes,
		private readonly _root: ThemeTrieElement
	) {}

	array<str> getColorMap() {
		return this._colorMap.getColorMap();
	}

	StyleAttributes getDefaults() {
		return this._defaults;
	}

	public match(scopePath: ScopeStack | null): StyleAttributes | null {
		if (scopePath === null) {
			return this._defaults;
		}
		const scopeName = scopePath.scopeName;
		const matchingTrieElements = this._cachedMatchRoot.get(scopeName);

		const effectiveRule = matchingTrieElements.find((v) =>
			_scopePathMatchesParentScopes(scopePath.parent, v.parentScopes)
		);
		if (!effectiveRule) {
			return null;
		}

		return new StyleAttributes(
			effectiveRule.fontStyle,
			effectiveRule.foreground,
			effectiveRule.background
		);
	}
}


struct SyncRegistry : IGrammarRepository, IThemeProvider {
	ion::map<Grammar> _grammars;
	ion::map<IRawGrammar> _rawGrammars;
	ion::map<array<ScopeName>> _injectionGrammars;

	private _theme: Theme;

	constructor(theme: Theme, private readonly _onigLibPromise: Promise<IOnigLib>) {
		this._theme = theme;
	}

	void dispose() {
		for (const grammar of this._grammars.values()) {
			grammar.dispose();
		}
	}

	void setTheme(theme: Theme) {
		this._theme = theme;
	}

	array<str> getColorMap() {
		return this._theme.getColorMap();
	}

	/**
	 * Add `grammar` to registry and return a list of referenced scope names
	 */
	public addGrammar(grammar: IRawGrammar, injectionScopeNames?: ScopeName[]): void {
		this._rawGrammars.set(grammar.scopeName, grammar);

		if (injectionScopeNames) {
			this._injectionGrammars.set(grammar.scopeName, injectionScopeNames);
		}
	}

	/**
	 * Lookup a raw grammar.
	 */
	IRawGrammar lookup(scopeName: ScopeName) {
		return this._rawGrammars.get(scopeName)!;
	}

	/**
	 * Returns the injections for the given grammar
	 */
	array<ScopeName> injections(targetScope: ScopeName) {
		return this._injectionGrammars.get(targetScope)!;
	}

	/**
	 * Get the default theme settings
	 */
	StyleAttributes getDefaults() {
		return this._theme.getDefaults();
	}

	/**
	 * Match a scope in the theme.
	 */
	StyleAttributes themeMatch(scopePath: ScopeStack) {
		return this._theme.match(scopePath);
	}

	/**
	 * Lookup a grammar.
	 */
	public async grammarForScopeName(
		scopeName: ScopeName,
		initialLanguage: number,
		embeddedLanguages: IEmbeddedLanguagesMap | null,
		tokenTypes: ITokenTypeMap | null,
		balancedBracketSelectors: BalancedBracketSelectors | null
	): Promise<IGrammar | null> {
		if (!this._grammars.has(scopeName)) {
			let rawGrammar = this._rawGrammars.get(scopeName)!;
			if (!rawGrammar) {
				return null;
			}
			this._grammars.set(scopeName, createGrammar(
				scopeName,
				rawGrammar,
				initialLanguage,
				embeddedLanguages,
				tokenTypes,
				balancedBracketSelectors,
				this,
				await this._onigLibPromise
			));
		}
		return this._grammars.get(scopeName)!;
	}
}



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

	virtual void collectPatterns(IRuleRegistry grammar, RegExpSourceList &out) = 0;
	virtual CompiledRule compile(IRuleRegistry grammar, utf16 endRegexSource) = 0;
	virtual CompiledRule compileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) = 0;
}

struct ICompilePatternsResult {
	array<RuleId> patterns;
	bool hasMissingPatterns;
};

class CaptureRule : Rule {

	RuleId retokenizeCapturedWithRuleId;

	CaptureRule(ILocation _location, RuleId id, utf16 name, utf16 contentName, RuleId retokenizeCapturedWithRuleId) {
		super(_location, id, name, contentName);
		data->retokenizeCapturedWithRuleId = retokenizeCapturedWithRuleId;
	}

	void dispose() {
		// nothing to dispose
	}

	void collectPatterns(IRuleRegistry grammar, RegExpSourceList &out) {
		throw new Error('Not supported!');
	}

	CompiledRule compile(IRuleRegistry grammar, utf16 endRegexSource) {
		throw new Error('Not supported!');
	}

	CompiledRule compileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		throw new Error('Not supported!');
	}
}

struct MatchRule:Rule {
	RegExpSource _match;
	CaptureRule captures[];
	RegExpSourceList _cachedCompiledPatterns;

	constructor(ILocation _location, RuleId id, utf16 name, utf16 match, CaptureRule captures[]) {
		super(_location, id, name, null);
		data->_match = new RegExpSource(match, data->id);
		data->captures = captures;
		data->_cachedCompiledPatterns = null;
	}

	void dispose() {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
	}

	public get debugMatchRegExp(): utf16 {
		return `${data->_match.source}`;
	}

	public collectPatterns(IRuleRegistry grammar, RegExpSourceList out) {
		out.push(data->_match);
	}

	CompiledRule compile(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	CompiledRule compileAG(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	RegExpSourceList _getCachedCompiledPatterns(IRuleRegistry grammar & IOnigLib) {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();
			data->collectPatterns(grammar, data->_cachedCompiledPatterns);
		}
		return data->_cachedCompiledPatterns;
	}
}

struct IncludeOnlyRule:Rule {
	bool hasMissingPatterns;
	array<RuleId> patterns;
	private RegExpSourceList _cachedCompiledPatterns;

	constructor(ILocation _location, RuleId id, utf16 name, utf16 contentName, ICompilePatternsResult patterns) {
		super(_location, id, name, contentName);
		data->patterns = patterns.patterns;
		data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
	}

	void dispose() {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
	}

	void collectPatterns(IRuleRegistry grammar, RegExpSourceList &out) {
		for (const pattern of data->patterns) {
			const rule = grammar.getRule(pattern);
			rule.collectPatterns(grammar, out);
		}
	}

	CompiledRule compile(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	CompiledRule compileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	RegExpSourceList _getCachedCompiledPatterns(IRuleRegistry grammar & IOnigLib) {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();
			data->collectPatterns(grammar, data->_cachedCompiledPatterns);
		}
		return data->_cachedCompiledPatterns;
	}
}

struct BeginEndRule:Rule {
	RegExpSource _begin;
	CaptureRule beginCaptures[];
	RegExpSource _end;
	bool endHasBackReferences;
	CaptureRule endCaptures[];
	bool applyEndPatternLast;
	bool hasMissingPatterns;
	RuleId patterns[];
	private RegExpSourceList _cachedCompiledPatterns;

	constructor(ILocation _location, RuleId id, utf16 name, utf16 contentName, utf16 begin, CaptureRule beginCaptures[], utf16 end, CaptureRule endCaptures[], bool applyEndPatternLast, ICompilePatternsResult patterns) {
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

	void dispose() {
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

	utf16 getEndWithResolvedBackReferences(utf16 lineText, IOnigCaptureIndex captureIndices[]) {
		return data->_end.resolveBackReferences(lineText, captureIndices);
	}

	public collectPatterns(IRuleRegistry grammar, RegExpSourceList out) {
		out.push(data->_begin);
	}

	CompiledRule compile(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar, endRegexSource).compile(grammar);
	}

	CompiledRule compileAG(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar, endRegexSource).compileAG(grammar, allowA, allowG);
	}

	RegExpSourceList _getCachedCompiledPatterns(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource) {
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

struct BeginWhileRule : Rule {
	RegExpSource _begin;
	CaptureRule beginCaptures[];
	CaptureRule whileCaptures[];
	RegExpSource _while<RuleId>;
	bool whileHasBackReferences;
	bool hasMissingPatterns;
	RuleId patterns[];
	private RegExpSourceList _cachedCompiledPatterns;
	private RegExpSourceList _cachedCompiledWhilePatterns<RuleId> | null;

	BeginWhileRule(
			ILocation _location, RuleId id, utf16 name, utf16 contentName, 
			utf16 begin, array<CaptureRule> beginCaptures, utf16 _while,
			array<CaptureRule> whileCaptures, ICompilePatternsResult patterns) : Rule(_location, id, name, contentName) {
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

	void dispose() {
		if (data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns.dispose();
			data->_cachedCompiledPatterns = null;
		}
		if (data->_cachedCompiledWhilePatterns) {
			data->_cachedCompiledWhilePatterns.dispose();
			data->_cachedCompiledWhilePatterns = null;
		}
	}

	utf16 get debugBeginRegExp() {
		return `${data->_begin.source}`;
	}

	utf16 get debugWhileRegExp() {
		return `${data->_while.source}`;
	}

	utf16 getWhileWithResolvedBackReferences(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
		return data->_while.resolveBackReferences(lineText, captureIndices);
	}

	void collectPatterns(IRuleRegistry grammar, RegExpSourceList &out) {
		out.push(data->_begin);
	}

	CompiledRule compile(IRuleRegistry grammar, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(grammar);
	}

	CompiledRule compileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(grammar, allowA, allowG);
	}

	RegExpSourceList _getCachedCompiledPatterns(IRuleRegistry grammar) {
		if (!data->_cachedCompiledPatterns) {
			data->_cachedCompiledPatterns = new RegExpSourceList();

			for (const pattern of data->patterns) {
				const rule = grammar.getRule(pattern);
				rule.collectPatterns(grammar, data->_cachedCompiledPatterns);
			}
		}
		return data->_cachedCompiledPatterns;
	}

	CompiledRule compileWhile(IRuleRegistry grammar, utf16 endRegexSource) {
		return data->_getCachedCompiledWhilePatterns(grammar, endRegexSource).compile(grammar);
	}

	CompiledRule compileWhileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledWhilePatterns(grammar, endRegexSource).compileAG(grammar, allowA, allowG);
	}

	RegExpSourceList<RuleId> _getCachedCompiledWhilePatterns(IRuleRegistry grammar & IOnigLib, utf16 endRegexSource) {
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

struct RuleFactory {

	public static createCaptureRule(IRuleFactoryHelper helper, ILocation _location, utf16 name, utf16 contentName, RuleId retokenizeCapturedWithRuleId): CaptureRule {
		return helper.registerRule((id) => {
			return new CaptureRule(_location, id, name, contentName, retokenizeCapturedWithRuleId);
		});
	}

	public static getCompiledRuleId(IRawRule desc, IRuleFactoryHelper helper, IRawRepository repository): RuleId {
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

	private static _compileCaptures(IRawCaptures captures, IRuleFactoryHelper helper, IRawRepository repository): CaptureRule[] {
		let CaptureRule r[] = [];

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
				let RuleId retokenizeCapturedWithRuleId = 0;
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
			.patterns = r,
			.hasMissingPatterns = ((patterns ? patterns.len() : 0) != r.len())
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
	CompiledRule A0_G0;
	CompiledRule A0_G1;
	CompiledRule A1_G0;
	CompiledRule A1_G1;
}

struct RegExpSourceList<TRuleId = RuleId | typeof endRuleId> {

	array<RegExpSource> _items;
	private bool _hasAnchors;
	private CompiledRule _cached;
	private IRegExpSourceListAnchorCache _anchorCache;

	constructor() {
		data->_items = [];
		data->_hasAnchors = false;
		data->_cached = null;
		data->_anchorCache = {
			.A0_G0 = null,
			.A0_G1 = null,
			.A1_G0 = null,
			.A1_G1 = null
		};
	}

	void dispose() {
		_disposeCaches();
	}

	void _disposeCaches() {
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

	void push(RegExpSource item) {
		data->_items.push(item);
		data->_hasAnchors = data->_hasAnchors || item.hasAnchor;
	}

	void unshift(RegExpSource item) {
		data->_items.unshift(item);
		data->_hasAnchors = data->_hasAnchors || item.hasAnchor;
	}

	num length() {
		return data->_items.len();
	}

	void setSource(num index, utf16 newSource) {
		if (data->_items[index].source != newSource) {
			// bust the cache
			data->_disposeCaches();
			data->_items[index].setSource(newSource);
		}
	}

	CompiledRule compile(IOnigLib onigLib) {
		if (!data->_cached) {
			let regExps = data->_items.map(e => e.source);
			data->_cached = new CompiledRule<TRuleId>(onigLib, regExps, data->_items.map(e => e.ruleId));
		}
		return data->_cached;
	}

	CompiledRule<TRuleId> compileAG(IOnigLib onigLib, bool allowA, bool allowG) {
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

	CompiledRule<TRuleId> _resolveAnchors(IOnigLib onigLib, bool allowA, bool allowG) {
		let regExps = data->_items.map(e => e.resolveAnchors(allowA, allowG));
		return new CompiledRule(onigLib, regExps, data->_items.map(e => e.ruleId));
	}
}

struct CompiledRule {
	OnigScanner scanner;

	constructor(IOnigLib onigLib, array<utf16> regExps, array<RuleId> rules) : regExps(regExps), rules(rules) {
		data->scanner = onigLib.createOnigScanner(regExps);
	}

	void dispose() {
		if (typeof data->scanner.dispose == "function") {
			data->scanner.dispose();
		}
	}

	toString(): utf16 {
		const array<utf16> r;
		for (let i = 0, len = data->rules.len(); i < len; i++) {
			r.push("   - " + data->rules[i] + ": " + data->regExps[i]);
		}
		return r.join("\n");
	}

	findNextMatchSync(
		utf16 string,
		num startPosition,
		states<FindOption> options
	): IFindNextMatchResult<TRuleId> {
		const result = data->scanner.findNextMatchSync(string, startPosition, options);
		if (!result) {
			return null;
		}

		return {
			.ruleId = data->rules[result.index],
			.captureIndices = result.captureIndices,
		};
	}
}

export interface IFindNextMatchResult<TRuleId = RuleId | typeof endRuleId> {
	TRuleId ruleId;
	IOnigCaptureIndex captureIndices[];
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





export class AttributedScopeStack {
	struct members {
		AttributedScopeStack parent;
		ScopeStack scopePath;
		EncodedTokenAttributes tokenAttributes;
	};
	mx_object(AttributedScopeStack, mx, members);

	static fromExtension(namesScopeList: AttributedScopeStack | null, contentNameScopesList: AttributedScopeStackFrame[]): AttributedScopeStack | null {
		let current = namesScopeList;
		let scopeNames = namesScopeList?.scopePath ?? null;
		for (const frame of contentNameScopesList) {
			scopeNames = ScopeStack.push(scopeNames, frame.scopeNames);
			current = new AttributedScopeStack(current, scopeNames!, frame.encodedTokenAttributes);
		}
		return current;
	}

	public static createRoot(scopeName: ScopeName, tokenAttributes: EncodedTokenAttributes): AttributedScopeStack {
		return new AttributedScopeStack(null, new ScopeStack(null, scopeName), tokenAttributes);
	}

	public static createRootAndLookUpScopeName(scopeName: ScopeName, tokenAttributes: EncodedTokenAttributes, grammar: Grammar): AttributedScopeStack {
		const rawRootMetadata = grammar.getMetadataForScope(scopeName);
		const scopePath = new ScopeStack(null, scopeName);
		const rootStyle = grammar.themeProvider.themeMatch(scopePath);

		const resolvedTokenAttributes = AttributedScopeStack.mergeAttributes(
			tokenAttributes,
			rawRootMetadata,
			rootStyle
		);

		return new AttributedScopeStack(null, scopePath, resolvedTokenAttributes);
	}

	public get scopeName(): ScopeName { return this.scopePath.scopeName; }

	/**
	 * Invariant:
	 * ```
	 * if (parent && !scopePath.extends(parent.scopePath)) {
	 * 	throw new Error();
	 * }
	 * ```
	 */
	private constructor(
		public readonly parent: AttributedScopeStack | null,
		public readonly scopePath: ScopeStack,
		public readonly tokenAttributes: EncodedTokenAttributes
	) {
	}

	public toString() {
		return this.getScopeNames().join(' ');
	}

	boolean equals(other: AttributedScopeStack) {
		return AttributedScopeStack.equals(this, other);
	}

	public static equals(
		a: AttributedScopeStack | null,
		b: AttributedScopeStack | null
	): boolean {
		do {
			if (a === b) {
				return true;
			}

			if (!a && !b) {
				// End of list reached for both
				return true;
			}

			if (!a || !b) {
				// End of list reached only for one
				return false;
			}

			if (a.scopeName !== b.scopeName || a.tokenAttributes !== b.tokenAttributes) {
				return false;
			}

			// Go to previous pair
			a = a.parent;
			b = b.parent;
		} while (true);
	}

	private static mergeAttributes(
		existingTokenAttributes: EncodedTokenAttributes,
		basicScopeAttributes: BasicScopeAttributes,
		styleAttributes: StyleAttributes | null
	): EncodedTokenAttributes {
		let fontStyle = FontStyle.NotSet;
		let foreground = 0;
		let background = 0;

		if (styleAttributes !== null) {
			fontStyle = styleAttributes.fontStyle;
			foreground = styleAttributes.foregroundId;
			background = styleAttributes.backgroundId;
		}

		return EncodedTokenAttributes.set(
			existingTokenAttributes,
			basicScopeAttributes.languageId,
			basicScopeAttributes.tokenType,
			null,
			fontStyle,
			foreground,
			background
		);
	}

	public pushAttributed(scopePath: ScopePath | null, grammar: Grammar): AttributedScopeStack {
		if (scopePath === null) {
			return this;
		}

		if (scopePath.indexOf(' ') === -1) {
			// This is the common case and much faster

			return AttributedScopeStack._pushAttributed(this, scopePath, grammar);
		}

		const scopes = scopePath.split(/ /g);
		let result: AttributedScopeStack = this;
		for (const scope of scopes) {
			result = AttributedScopeStack._pushAttributed(result, scope, grammar);
		}
		return result;

	}

	static AttributedScopeStack _pushAttributed(
		AttributedScopeStack target,
		ScopeName scopeName,
		Grammar grammar,
	) {
		const rawMetadata = grammar.getMetadataForScope(scopeName);

		const newPath = target.scopePath.push(scopeName);
		const scopeThemeMatchResult =
			grammar.themeProvider.themeMatch(newPath);
		const metadata = AttributedScopeStack.mergeAttributes(
			target.tokenAttributes,
			rawMetadata,
			scopeThemeMatchResult
		);
		return AttributedScopeStack(target, newPath, metadata);
	}

	array<str> getScopeNames() {
		return this.scopePath.getSegments();
	}

	array<AttributedScopeStackFrame> getExtensionIfDefined(base: AttributedScopeStack | null) {
		const result: AttributedScopeStackFrame[] = [];
		let self: AttributedScopeStack | null = this;

		while (self && self !== base) {
			result.push({
				encodedTokenAttributes: self.tokenAttributes,
				scopeNames: self.scopePath.getExtensionIfDefined(self.parent?.scopePath ?? null)!,
			});
			self = self.parent;
		}
		return self === base ? result.reverse() : undefined;
	}
}

struct AttributedScopeStackFrame {
	num encodedTokenAttributes;
	array<str> scopeNames;
};





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

	bool static _equals(StateStackImpl a, StateStackImpl b) {
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

	Rule getRule(IRuleRegistry grammar) {
		return grammar.getRule(data->ruleId);
	}

	utf16 toString() {
		const utf16 r[] = [];
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

	StateStackImpl withEndRule(utf16 endRule) {
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
			ruleId = ruleIdToNumber(data->ruleId),
			beginRuleCapturedEOL = data->beginRuleCapturedEOL,
			endRule = data->endRule,
			nameScopesList = data->nameScopesList?.getExtensionIfDefined(data->parent?.nameScopesList ?? null)! ?? [],
			contentNameScopesList = data->contentNameScopesList?.getExtensionIfDefined(data->nameScopesList)! ?? [],
		};
	}

	static pushFrame(StateStackImpl self, StateStackFrame frame): StateStackImpl {
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
		bool emitBinaryTokens,
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

	void produce(StateStackImpl stack, num endIndex) {
		data->produceFromScopes(stack.contentNameScopesList, endIndex);
	}

	void produceFromScopes(
		AttributedScopeStack scopesList,
		num endIndex
	) {
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
			startIndex = data->_lastTokenEndIndex,
			endIndex   = endIndex,
			// value   = lineText.mid(lastTokenEndIndex, endIndex),
			scopes     = scopes
		});

		data->_lastTokenEndIndex = endIndex;
	}

	IToken[] getResult(StateStackImpl stack, num lineLength) {
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

	Uint32Array getBinaryResult(StateStackImpl stack, num lineLength) {
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
		//BasicScopeAttributesProvider _basicScopeAttributesProvider;
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
						matcher = matcher.matcher,
						type = tokenTypes[selector],
					});
				}
			}
		}
	}

	BasicScopeAttributes getMetadataForScope(scope: string) {
		return data->_basicScopeAttributesProvider.getBasicScopeAttributes(scope);
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
			tokens = 		r.lineTokens.getResult(r.ruleStack, r.lineLength),
			ruleStack = 	r.ruleStack,
			stoppedEarly = 	r.stoppedEarly,
		};
	}

	ITokenizeLineResult2 tokenizeLine2(
			utf16 			lineText,
			StateStackImpl 	prevState,
			num 			timeLimit = 0) {
		const r = data->_tokenize(lineText, prevState, true, timeLimit);
		return {
			tokens = 		r.lineTokens.getBinaryResult(r.ruleStack, r.lineLength),
			ruleStack = 	r.ruleStack,
			stoppedEarly = 	r.stoppedEarly,
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
			//const rawDefaultMetadata =
			//	data->_basicScopeAttributesProvider.getDefaultAttributes();
			const defaultStyle = data->themeProvider.getDefaults();
			const defaultMetadata = EncodedTokenAttributes.set(
				0,
				0, // null rawDefaultMetadata.languageId,
				null, // rawDefaultMetadata.tokenType
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

	return { .stack = stack, .linePos = linePos, .anchorPosition = anchorPosition, .isFirstLine = isFirstLine };
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

	void scanNext() {
		if (DebugFlags.InDebugMode) {
			console.log("");
			//console.log(
			//	`@@scanNext ${linePos}: |${lineText.content
			//		.substr(linePos)
			//		.replace(/\n$/, "\\n")}|`
			//);
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

		const IOnigCaptureIndex captureIndices[] = r.captureIndices;
		const matchedRuleId = r.matchedRuleId;

		const hasAdvanced =
			captureIndices && captureIndices.len() > 0
				? captureIndices[0].end > linePos
				: false;

		if (matchedRuleId == endRuleId) {
			// We matched the `end` for this rule => pop it
			BeginEndRule poppedRule = stack.getRule(grammar);

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
			const nameScopesList = stack.contentNameScopesList.pushAttributed(
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

#if 0

template <typename... Ts>
struct com;

template <typename T>
using rr = std::reference_wrapper<T>;

/// the memory should all be reference counted the same
/// so the idea of pushing lots of these refs per class is not ideal.  they are used at the same time
/// there must be 1 memory!
/// its a bit too much to change atm.

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

	template <typename TT, typename... TTs>
	memory *tsize() {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return sizeof(TTs);
	}

	/// we want this method to do all allocation for the types
	template <typename TT, typename... TTs>
	TT *alloc_next(TT *src, TTs *src_next...) {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;
	}

	/// we want this method to do all allocation for the types
	template <typename TT, typename... TTs>
	TT *alloc(TT *src, TTs *src_next...) {
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

		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;

		mtx.unlock();
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
#endif


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
