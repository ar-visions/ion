#include <ux/app.hpp>

using namespace ion;

#if 1

const RegEx HAS_BACK_REFERENCES  = RegEx(utf16(R"(\\(\d+))"), RegEx::Behaviour::none);
const RegEx BACK_REFERENCING_END = RegEx(utf16(R"(\\(\d+))"), RegEx::Behaviour::global);

using ScopeName = str;
using ScopePath = str;
using ScopePattern = str;
using Uint32Array  = array<uint32_t>;



export type EncodedTokenAttributes = number;

namespace EncodedTokenAttributes {
	str toBinaryStr(encodedTokenAttributes: EncodedTokenAttributes) {
		return encodedTokenAttributes.toString(2).padStart(32, "0");
	}

	void print(encodedTokenAttributes: EncodedTokenAttributes) {
		const languageId = EncodedTokenAttributes::getLanguageId(encodedTokenAttributes);
		const tokenType = EncodedTokenAttributes::getTokenType(encodedTokenAttributes);
		const fontStyle = EncodedTokenAttributes::getFontStyle(encodedTokenAttributes);
		const foreground = EncodedTokenAttributes::getForeground(encodedTokenAttributes);
		const background = EncodedTokenAttributes::getBackground(encodedTokenAttributes);

		console.log({
			languageId: languageId,
			tokenType: tokenType,
			fontStyle: fontStyle,
			foreground: foreground,
			background: background,
		});
	}

	export function getLanguageId(encodedTokenAttributes: EncodedTokenAttributes): number {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::LANGUAGEID_MASK) >>>
			EncodedTokenDataConsts::LANGUAGEID_OFFSET
		);
	}

	export function getTokenType(encodedTokenAttributes: EncodedTokenAttributes): StandardTokenType {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::TOKEN_TYPE_MASK) >>>
			EncodedTokenDataConsts::TOKEN_TYPE_OFFSET
		);
	}

	export function containsBalancedBrackets(encodedTokenAttributes: EncodedTokenAttributes): boolean {
		return (encodedTokenAttributes & EncodedTokenDataConsts::BALANCED_BRACKETS_MASK) !== 0;
	}

	export function getFontStyle(encodedTokenAttributes: EncodedTokenAttributes): number {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::FONT_STYLE_MASK) >>>
			EncodedTokenDataConsts::FONT_STYLE_OFFSET
		);
	}

	export function getForeground(encodedTokenAttributes: EncodedTokenAttributes): number {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::FOREGROUND_MASK) >>>
			EncodedTokenDataConsts::FOREGROUND_OFFSET
		);
	}

	export function getBackground(encodedTokenAttributes: EncodedTokenAttributes): number {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::BACKGROUND_MASK) >>>
			EncodedTokenDataConsts::BACKGROUND_OFFSET
		);
	}

	/**
	 * Updates the fields in `metadata`.
	 * A value of `0`, `NotSet` or `null` indicates that the corresponding field should be left as is.
	 */
	export function set(
		encodedTokenAttributes: EncodedTokenAttributes,
		languageId: number | 0,
		tokenType: OptionalStandardTokenType | OptionalStandardTokenType.NotSet,
		containsBalancedBrackets: boolean | null,
		fontStyle: FontStyle | FontStyle.NotSet,
		foreground: number | 0,
		background: number | 0
	): number {
		let _languageId = EncodedTokenAttributes::getLanguageId(encodedTokenAttributes);
		let _tokenType = EncodedTokenAttributes::getTokenType(encodedTokenAttributes);
		let _containsBalancedBracketsBit: 0 | 1 =
			EncodedTokenAttributes::containsBalancedBrackets(encodedTokenAttributes) ? 1 : 0;
		let _fontStyle = EncodedTokenAttributes::getFontStyle(encodedTokenAttributes);
		let _foreground = EncodedTokenAttributes::getForeground(encodedTokenAttributes);
		let _background = EncodedTokenAttributes::getBackground(encodedTokenAttributes);

		if (languageId !== 0) {
			_languageId = languageId;
		}
		if (tokenType !== OptionalStandardTokenType.NotSet) {
			_tokenType = fromOptionalTokenType(tokenType);
		}
		if (containsBalancedBrackets !== null) {
			_containsBalancedBracketsBit = containsBalancedBrackets ? 1 : 0;
		}
		if (fontStyle !== FontStyle.NotSet) {
			_fontStyle = fontStyle;
		}
		if (foreground !== 0) {
			_foreground = foreground;
		}
		if (background !== 0) {
			_background = background;
		}

		return (
			((_languageId << EncodedTokenDataConsts::LANGUAGEID_OFFSET) |
				(_tokenType << EncodedTokenDataConsts::TOKEN_TYPE_OFFSET) |
				(_containsBalancedBracketsBit <<
					EncodedTokenDataConsts::BALANCED_BRACKETS_OFFSET) |
				(_fontStyle << EncodedTokenDataConsts::FONT_STYLE_OFFSET) |
				(_foreground << EncodedTokenDataConsts::FOREGROUND_OFFSET) |
				(_background << EncodedTokenDataConsts::BACKGROUND_OFFSET)) >>>
			0
		);
	}
}

/**
 * Helpers to manage the "collapsed" metadata of an entire StackElement stack.
 * The following assumptions have been made:
 *  - languageId < 256 => needs 8 bits
 *  - unique color count < 512 => needs 9 bits
 *
 * The binary format is:
 * - -------------------------------------------
 *     3322 2222 2222 1111 1111 1100 0000 0000
 *     1098 7654 3210 9876 5432 1098 7654 3210
 * - -------------------------------------------
 *     xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
 *     bbbb bbbb ffff ffff fFFF FBTT LLLL LLLL
 * - -------------------------------------------
 *  - L = LanguageId (8 bits)
 *  - T = StandardTokenType (2 bits)
 *  - B = Balanced bracket (1 bit)
 *  - F = FontStyle (4 bits)
 *  - f = foreground color (9 bits)
 *  - b = background color (9 bits)
 */
enum EncodedTokenDataConsts {
	LANGUAGEID_MASK = 0b00000000000000000000000011111111,
	TOKEN_TYPE_MASK = 0b00000000000000000000001100000000,
	BALANCED_BRACKETS_MASK = 0b00000000000000000000010000000000,
	FONT_STYLE_MASK = 0b00000000000000000111100000000000,
	FOREGROUND_MASK = 0b00000000111111111000000000000000,
	BACKGROUND_MASK = 0b11111111000000000000000000000000,

	LANGUAGEID_OFFSET = 0,
	TOKEN_TYPE_OFFSET = 8,
	BALANCED_BRACKETS_OFFSET = 10,
	FONT_STYLE_OFFSET = 11,
	FOREGROUND_OFFSET = 15,
	BACKGROUND_OFFSET = 24
};

enum StandardTokenType {
	Other = 0,
	Comment = 1,
	String = 2,
	RegEx = 3
};

OptionalStandardTokenType toOptionalTokenType(StandardTokenType standardType) {
	return (OptionalStandardTokenType)standardType;
}

StandardTokenType fromOptionalTokenType(OptionalStandardTokenType standardType) {
	return (StandardTokenType)standardType;
}

// Must have the same values as `StandardTokenType`!
enum OptionalStandardTokenType {
	Other = 0,
	Comment = 1,
	String = 2,
	RegEx = 3,
	// Indicates that no token type is set.
	NotSet = 8
}


bool _matchesScope(ScopeName scopeName, ScopeName scopePattern) {
	return scopePattern == scopeName || (scopeName.has_prefix(scopePattern) && scopeName[scopePattern.len()] == '.');
}

map<mx> mergeObjects(map<mx> dst, map<mx> src0, map<mx> src1) {
	for (field<mx> &f: src0) {
		dst[f.key] = f.value;
	}
	for (field<mx> &f: src1) {
		dst[f.key] = f.value;
	}
	return dst;
}

bool isValidHexColor(str hex) {
	if (!hex) return false;
    RegEx rgbRegex     ("^#[0-9a-fA-F]{3}$");
    RegEx rgbaRegex    ("^#[0-9a-fA-F]{4}$");
    RegEx rrggbbRegex  ("^#[0-9a-fA-F]{6}$");
    RegEx rrggbbaaRegex("^#[0-9a-fA-F]{8}$");

    return rgbRegex.exec(hex)    || 
           rgbaRegex.exec(hex)   ||
           rrggbbRegex.exec(hex) ||
           rrggbbaaRegex.exec(hex);
}

enum FontStyle {
	NotSet = -1,
	None = 0,
	Italic = 1,
	Bold = 2,
	Underline = 4,
	Strikethrough = 8
};

/**
 * A map from scope name to a language id. Please do not use language id 0.
 */
using IEmbeddedLanguagesMap = map<num>;

using ITokenTypeMap = map<str>;

struct ILocation {
	str 			filename;
	num 			line;
	num 			chr;
};

struct ILocatable {
	ILocation 		_vscodeTextmateLocation;
};

using RuleId = num;

struct IRawRule;
using IRawCapturesMap = map<IRawRule*>;
using IRawCaptures    = IRawCapturesMap; // not wanting to use ILocatable metadata

struct IRawRepositoryMap {
	map<mx> 		props; /// of type IRawRule
	sp<IRawRule>	_self;
	sp<IRawRule>	_base;
};

struct IRawRepository : ILocatable, IRawRepositoryMap { };

struct IRawRule {
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
	IRawRepository 		repository;
	bool 				applyEndPatternLast;
};

struct IRawGrammar : ILocatable {
	IRawRepository 		repository;
	ScopeName 			scopeName;
	array<IRawRule> 	patterns;
	map<IRawRule> 		injections;
	str 				injectionSelector;
	array<str> 			fileTypes;
	str 				name;
	str 				firstLineMatch;
};

struct IGrammarRepository {
	virtual IRawGrammar 	 lookup(ScopeName scopeName) = 0;
	virtual array<ScopeName> injections(ScopeName scopeName) = 0;
};

struct RawThemeStyles:mx {
	struct members {
		str fontStyle;
		str foreground;
		str background;
	};
	mx_basic(RawThemeStyles);
};

struct IRawThemeSetting {
	str 			name;
	mx 				scope; // can be ScopePattern or ScopePattern[] ! (so string or array of them)
	RawThemeStyles  styles;
};

struct IRawTheme {
	str name;
	array<IRawThemeSetting> settings;
};

struct RegistryOptions {
	IRawTheme theme;
	array<str> colorMap;
	future loadGrammar(ScopeName scopeName);
	lambda<array<ScopeName>(ScopeName)> getInjections;
};

struct ScopeStack:mx {
	struct members {
		memory 	   *m_parent;
		ScopeStack *parent;
		ScopeName 	scopeName;
	};

	mx_basic(ScopeStack);

	static ScopeStack push(ScopeStack path, array<ScopeName> scopeNames) {
		for (auto &name:scopeNames) {
			path = ScopeStack(path, name);
		}
		return path;
	}

	//public static from(first: ScopeName, ...segments: ScopeName[]): ScopeStack;

	//public static from(...segments: ScopeName[]): ScopeStack;

	//public static from(...segments: ScopeName[]): ScopeStack {
	//	let result: ScopeStack = null;
	//	for (let i = 0; i < segments.len(); i++) {
	//		result = ScopeStack(result, segments[i]);
	//	}
	//	return result;
	//}

	ScopeStack(ScopeStack &parent, ScopeName scopeName) : ScopeStack() {
		data->m_parent  = parent.grab();
		data->parent    = &parent;
		data->scopeName = scopeName;
	}

	ScopeStack push(ScopeName scopeName) {
		return ScopeStack(*this, scopeName);
	}

	array<ScopeName> getSegments() {
		ScopeStack item = *this;
		array<ScopeName> result;
		while (item) {
			result.push(item->scopeName);
			if (!item->parent)
				break;
			item = *item->parent;
		}
		return result.reverse();
	}

	str toString() {
		return getSegments().join(' ');
	}

	bool extends(ScopeStack other) {
		if (mem == other.mem) {
			return true;
		}
		if (data->parent == null) {
			return false;
		}
		return data->parent->extends(other);
	}

	array<str> getExtensionIfDefined(ScopeStack base) {
		array<str> result;
		ScopeStack item = *this;
		while (item && item != base) {
			result.push(item->scopeName);
			if (!item->parent)
				return {};
			item = *item->parent;
		}
		return result.reverse();
	}
};

bool _scopePathMatchesParentScopes(ScopeStack scopePath, array<ScopeName> parentScopes) {
	if (parentScopes == null) {
		return true;
	}

	num index = 0;
	str scopePattern = parentScopes[index];

	while (scopePath) {
		if (_matchesScope(scopePath->scopeName, scopePattern)) {
			index++;
			if (index == parentScopes.len()) {
				return true;
			}
			scopePattern = parentScopes[index];
		}
		scopePath = *scopePath->parent;
	}
	return false;
};

struct StyleAttributes:mx {
	struct members {
		states<FontStyle> fontStyle;
		num foregroundId;
		num backgroundId;
	};
	mx_basic(StyleAttributes);

	StyleAttributes(states<FontStyle> fontStyle, num foregroundId, num backgroundId):StyleAttributes() {
		data->fontStyle = fontStyle;
		data->foregroundId = foregroundId;
		data->backgroundId = backgroundId;
	}
};

/**
 * Parse a raw theme into rules.
 */

struct ParsedThemeRule:mx {
	struct members {
		ScopeName 			scope;
		array<ScopeName> 	parentScopes;
		num 				index;
		states<FontStyle> 	fontStyle;
		str 				foreground;
		str 				background;
	};
	mx_basic(ParsedThemeRule);
};

array<ParsedThemeRule> parseTheme(IRawTheme &source) {
	if (!source.settings)
		return {};
	array<IRawThemeSetting> &settings = source.settings;
	array<ParsedThemeRule> result;

	for (num i = 0, len = settings.len(); i < len; i++) {
		auto &entry = settings[i];

		if (!entry.styles) {
			continue;
		}

		array<str> scopes;
		if (entry.scope.type() == typeof(str)) {
			str _scope = entry.scope.grab();

			// remove leading commas
			_scope = _scope.replace(R"(^[,]+)", "");

			// remove trailing commans
			_scope = _scope.replace(R"([,]+$)", "");

			scopes = _scope.split(',');
		} else if (entry.scope.type()->traits & traits::array) {
			scopes = entry.scope.grab();
		} else {
			scopes = array<str> { "" };
		}

		states<FontStyle> fontStyle = {};
		if (entry.styles->fontStyle) {
			array<str> segments = entry.styles->fontStyle.split(" ");
			for (int j = 0, lenJ = segments.len(); j < lenJ; j++) {
				str &segment = segments[j];
				fontStyle[segment] = true;
			}
		}

		str foreground = null;
		if (isValidHexColor(entry.styles->foreground)) {
			foreground = entry.styles->foreground;
		}

		str background = null;
		if (isValidHexColor(entry.styles->background)) {
			background = entry.styles->background;
		}

		for (num j = 0, lenJ = scopes.len(); j < lenJ; j++) {
			str _scope = scopes[j].trim();

			array<str> segments = _scope.split(" ");

			str scope = segments[segments.len() - 1];
			array<str> parentScopes;
			if (segments.len() > 1) {
				parentScopes = segments.slice(0, segments.len() - 1);
				parentScopes.reverse();
			}
			ParsedThemeRule p = ParsedThemeRule::members {
				scope,
				parentScopes,
				i,
				fontStyle,
				foreground,
				background
			};
			result.push(p);

			// no overload on the pointer op, but overload on the '.' is possible
			//T operator.(SYMBOL s) { /// design-time symbol
			//}
		}
	}

	return result;
}

str fontStyleToString(states<FontStyle> fontStyle) {
	if (fontStyle == FontStyle::NotSet) {
		return "not set";
	}

	str style = "";
	if (fontStyle & FontStyle::Italic) {
		style += "italic ";
	}
	if (fontStyle & FontStyle::Bold) {
		style += "bold ";
	}
	if (fontStyle & FontStyle::Underline) {
		style += "underline ";
	}
	if (fontStyle & FontStyle::Strikethrough) {
		style += "strikethrough ";
	}
	if (style == "") {
		style = "none";
	}
	return style.trim();
};

/**
 * Resolve rules (i.e. inheritance).
 */
Theme resolveParsedThemeRules(array<ParsedThemeRule> parsedThemeRules, array<str> _colorMap) {

	// Sort rules lexicographically, and then by index if necessary
	parsedThemeRules.sort([](ParsedThemeRule &a, ParsedThemeRule &b) -> int {
		num r = strcmp(a->scope, b->scope);
		if (r != 0) {
			return r;
		}
		r = strArrCmp(a->parentScopes, b->parentScopes);
		if (r != 0) {
			return r;
		}
		return a->index - b->index;
	});

	// Determine defaults
	states<FontStyle> defaultFontStyle = FontStyle::None;
	str defaultForeground = "#000000";
	str defaultBackground = "#ffffff";
	while (parsedThemeRules.len() >= 1 && parsedThemeRules[0].scope == "") {
		ParsedThemeRule incomingDefaults = parsedThemeRules.shift()
		if (incomingDefaults.fontStyle != FontStyle::NotSet) {
			defaultFontStyle = incomingDefaults->fontStyle;
		}
		if (incomingDefaults.foreground != null) {
			defaultForeground = incomingDefaults->foreground;
		}
		if (incomingDefaults.background != null) {
			defaultBackground = incomingDefaults->background;
		}
	}
	ColorMap 		colorMap = ColorMap(_colorMap);
	StyleAttributes defaults = StyleAttributes(defaultFontStyle, colorMap.getId(defaultForeground), colorMap.getId(defaultBackground));

	ThemeTrieElement root    = ThemeTrieElement(ThemeTrieElementRule::members { 0, null, FontStyle::NotSet, 0, 0 }, {});
	for (num i = 0, len = parsedThemeRules.len(); i < len; i++) {
		ParsedThemeRule rule = parsedThemeRules[i];
		root.insert(0, rule.scope, rule.parentScopes, rule.fontStyle, colorMap.getId(rule.foreground), colorMap.getId(rule.background));
	}

	return Theme(colorMap, defaults, root);
}

struct ColorMap:mx {
	struct member {
		bool 		_isFrozen;
		num 		_lastColorId;
		array<str> 	_id2color;
		map<mx> 	_color2id;
	};

	ColorMap(array<str> _colorMap) {
		data->_lastColorId = 0;

		if (_colorMap) {
			data->_isFrozen = true;
			for (size_t i = 0, len = _colorMap.len(); i < len; i++) {
				data->_color2id[_colorMap[i]] = i;
				data->_id2color.push(_colorMap[i]);
			}
		} else {
			data->_isFrozen = false;
		}
	}

	num getId(str color) {
		if (color == null) {
			return 0;
		}
		color = color.toUpperCase();
		mx value = data->_color2id[color];
		if (value) {
			return value;
		}
		if (data->_isFrozen) {
			console.fault("Missing color in color map - {0}", {color});
		}
		value = ++data->_lastColorId;
		data->_color2id[color] = value;
		data->_id2color[value] = color;
		return value;
	}

	array<str> getColorMap() {
		return data->_id2color.slice(0);
	}
}

struct ThemeTrieElementRule:mx {
	struct members {
		num scopeDepth;
		array<ScopeName> parentScopes;
		num fontStyle;
		num foreground;
		num background;
	};
	mx_basic(ThemeTrieElementRule);

	ThemeTrieElementRule clone() {
		return ThemeTrieElementRule(data->scopeDepth, data->parentScopes, data->fontStyle, data->foreground, data->background);
	}

	static array<ThemeTrieElementRule> cloneArr(array<ThemeTrieElementRule> arr) {
		array<ThemeTrieElementRule> r;
		for (num i = 0, len = arr.len(); i < len; i++) {
			r[i] = arr[i].clone();
		}
		return r;
	}

	void acceptOverwrite(num scopeDepth, num fontStyle, num foreground, num background) {
		if (data->scopeDepth > scopeDepth) {
			console.log("how did this happen?");
		} else {
			data->scopeDepth = scopeDepth;
		}
		// console.log("TODO -> my depth: " + data->scopeDepth + ", overwriting depth: " + scopeDepth);
		if (fontStyle != FontStyle::NotSet) {
			data->fontStyle = fontStyle;
		}
		if (foreground != 0) {
			data->foreground = foreground;
		}
		if (background != 0) {
			data->background = background;
		}
	}
}

struct ThemeTrieElement:mx {
	using ITrieChildrenMap = map<ThemeTrieElement>;

	struct members {
		ThemeTrieElementRule 		_mainRule;
		array<ThemeTrieElementRule> _rulesWithParentScopes;
		ITrieChildrenMap 			_children;
	};

	ThemeTrieElement(
		ThemeTrieElementRule _mainRule,
		array<ThemeTrieElementRule> rulesWithParentScopes = {},
		ITrieChildrenMap _children = {}
	) : ThemeTrieElement() {
		data->_mainRule = _mainRule;
		data->_rulesWithParentScopes = rulesWithParentScopes;
		data->_children = _children;
	}

	static array<ThemeTrieElementRule> _sortBySpecificity(array<ThemeTrieElementRule> arr) {
		if (arr.len() == 1) {
			return arr;
		}
		arr.sort(data->_cmpBySpecificity);
		return arr;
	}

	static num _cmpBySpecificity(ThemeTrieElementRule a, ThemeTrieElementRule b) {
		if (a->scopeDepth == b->scopeDepth) {
			const aParentScopes = a->parentScopes;
			const bParentScopes = b->parentScopes;
			num aParentScopesLen = aParentScopes == null ? 0 : aParentScopes.len();
			num bParentScopesLen = bParentScopes == null ? 0 : bParentScopes.len();
			if (aParentScopesLen == bParentScopesLen) {
				for (num i = 0; i < aParentScopesLen; i++) {
					const aLen = aParentScopes![i].len();
					const bLen = bParentScopes![i].len();
					if (aLen != bLen) {
						return bLen - aLen;
					}
				}
			}
			return bParentScopesLen - aParentScopesLen;
		}
		return b->scopeDepth - a->scopeDepth;
	}

	array<ThemeTrieElementRule> default_result() {
		array<ThemeTrieElementRule> a;
		a.push(data->_mainRule);
		for (ThemeTrieElementRule &r: data->_rulesWithParentScope)
			a.push(r);
		return ThemeTrieElement._sortBySpecificity(a);
	}

	array<ThemeTrieElementRule> match(ScopeName scope) {
		if (scope == "")
			return default_result();
		int dotIndex = scope.indexOf(".");
		str head     = (dotIndex == -1) ? scope : scope.mid(0, dotIndex);
		str tail     = (dotIndex == -1) ? ""    : scope.mid(dotIndex + 1);
		if (data->_children.count(head))
			return data->_children[head].match(tail);
		return default_result();
	}

	void insert(num scopeDepth, ScopeName scope, array<ScopeName> parentScopes, num fontStyle, num foreground, num background) {
		if (scope == "") {
			data->_doInsertHere(scopeDepth, parentScopes, fontStyle, foreground, background);
			return;
		}
		int dotIndex = scope.indexOf(".");
		str head = (dotIndex == -1) ? scope : scope.mid(0, dotIndex);
		str tail = (dotIndex == -1) ? ""    : scope.mid(dotIndex + 1);
		ThemeTrieElement child;
		if (data->_children.count(head)) {
			child = data->_children[head];
		} else {
			child = ThemeTrieElement(
				data->_mainRule.clone(),
				ThemeTrieElementRule::cloneArr(data->_rulesWithParentScopes));
			data->_children[head] = child;
		}

		child.insert(scopeDepth + 1, tail, parentScopes, fontStyle, foreground, background);
	}

	void _doInsertHere(num scopeDepth, array<ScopeName> parentScopes, num fontStyle, num foreground, num background) {

		if (parentScopes == null) {
			// Merge into the main rule
			data->_mainRule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
			return;
		}

		// Try to merge into existing rule
		for (num i = 0, len = data->_rulesWithParentScopes.len(); i < len; i++) {
			ThemeTrieElementRule &rule = data->_rulesWithParentScopes[i];

			if (strArrCmp(rule.parentScopes, parentScopes) == 0) {
				// bingo! => we get to merge this into an existing one
				rule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
				return;
			}
		}

		// Must add a new rule

		// Inherit from main rule
		if (fontStyle == FontStyle::NotSet) {
			fontStyle = data->_mainRule.fontStyle;
		}
		if (foreground == 0) {
			foreground = data->_mainRule->foreground;
		}
		if (background == 0) {
			background = data->_mainRule->background;
		}

		data->_rulesWithParentScopes.push(
			ThemeTrieElementRule::members {
				scopeDepth, parentScopes, fontStyle, foreground, background
			}
		);
	}
};

using ITrieChildrenMap = map<ThemeTrieElement>;

struct Theme:mx {
	struct members {
		ColorMap 			_colorMap;
		StyleAttributes	 	_defaults;
		ThemeTrieElement 	_root;
		lambda<ThemeTriElementRule(ScopeName)> _cachedMatchRoot;
		members() {
			_cachedMatchRoot = [&](ScopeName scopeName) -> ThemeTriElementRule { return _root.match(scopeName); };
		}
	};
	mx_basic(Theme);

	static Theme createFromRawTheme(
		IRawTheme source,
		array<str> colorMap
	)  {
		return data->createFromParsedTheme(parseTheme(source), colorMap);
	}

	static Theme createFromParsedTheme(
		array<ParsedThemeRule> source,
		array<str> colorMap
	) {
		return resolveParsedThemeRules(source, colorMap);
	}



	array<str> getColorMap() {
		return data->_colorMap.getColorMap();
	}

	StyleAttributes getDefaults() {
		return data->_defaults;
	}

	StyleAttributes match(ScopeStack scopePath) {
		if (scopePath == null) {
			return data->_defaults;
		}
		const scopeName = scopePath.scopeName;
		const matchingTrieElements = data->_cachedMatchRoot.get(scopeName);

		const effectiveRule = matchingTrieElements.select_first([&](ThemeTrieElement &v) -> bool
			_scopePathMatchesParentScopes(scopePath->parent, v->parentScopes)
		);
		if (!effectiveRule) {
			return null;
		}
		return StyleAttributes(
			effectiveRule.fontStyle,
			effectiveRule.foreground,
			effectiveRule.background
		);
	}
};



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
	mx_basic(IncludeReference);
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

struct ExternalReferenceCollector:mx {
	struct members {
		array<TopLevelRuleReference> references;
		array<str>					_seenReferenceKeys;
		array<IRawRule> 			 visitedRule;
	};

	mx_basic(ExternalReferenceCollector)

	void add(TopLevelRuleReference reference) {
		const key = reference.toKey();
		if (this._seenReferenceKeys.index_of(key) >= 0) {
			return;
		}
		this._seenReferenceKeys.add(key);
		this._references.push(reference);
	}
};

struct TopLevelRuleReference:mx {
	struct members {
		ScopeName scopeName;
	};
	mx_basic(TopLevelRuleReference)
	
	TopLevelRuleReference(ScopeName scopeName) : TopLevelRuleReference() {
		data->scopeName = scopeName;
	}

	str toKey() {
		return data->scopeName;
	}
};

/**
 * References a rule of a grammar in the top level repository section with the given name.
*/
struct TopLevelRepositoryRuleReference:TopLevelRuleReference {
	struct members {
		string ruleName;
	};

	TopLevelRepositoryRuleReference(
		ScopeName 	scopeName,
		str 		ruleName) : TopLevelRepositoryRuleReference() {
		TopLevelRuleReference::data->scopeName = scopeName;
		data->ruleName = ruleName;
	}
	
	mx_object(TopLevelRepositoryRuleReference, TopLevelRuleReference)

	str toKey() {
		return fmt { "{0}#{1}", {
			TopLevelRuleReference::data->scopeName,
			TopLevelRepositoryRuleReference::data->ruleName} };
	}
}


void collectReferencesOfReference(
	TopLevelRuleReference 		reference,
	ScopeName 					baseGrammarScopeName,
	IGrammarRepository 			repo,
	ExternalReferenceCollector 	result)
{
	const selfGrammar = repo.lookup(reference->scopeName);
	if (!selfGrammar) {
		if (reference.scopeName == baseGrammarScopeName) {
			console.fault("No grammar provided for <{0}>", {baseGrammarScopeName});
		}
		return;
	}

	const baseGrammar = repo.lookup(baseGrammarScopeName)!;

	if (reference.type() == typeof(TopLevelRuleReference)) {
		collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, result);
	} else {
		collectExternalReferencesInTopLevelRepositoryRule(
			reference.ruleName, baseGrammar, selfGrammar, selfGrammar.repository,
			result
		);
	}

	const injections = repo.injections(reference.scopeName);
	if (injections) {
		for (const injection of injections) {
			result.add(TopLevelRuleReference(injection));
		}
	}
}

/// the interfaces we use multiple inheritence on the data member; thus isolating diamonds.
struct Context:mx {
	struct members {
		IRawGrammar baseGrammar;
		IRawGrammar selfGrammar;
	};
	mx_basic(Context);
};

struct ContextWithRepository {
	IRawGrammar   	baseGrammar;
	IRawGrammar   	selfGrammar;
	map<IRawRule> 	repository;
};

void collectExternalReferencesInTopLevelRepositoryRule(
	str 			ruleName;
	IRawGrammar    &baseGrammar;	/// omitted 'ContextWithRepository' and added the args here
	IRawGrammar    &selfGrammar;
	map<IRawRule> 	repository;
	ExternalReferenceCollector result;
) {
	if (repository && repository[ruleName]) {
		array<IRawRule> rules { repository[ruleName] };
		collectExternalReferencesInRules(rules, context, result);
	}
}

void collectExternalReferencesInTopLevelRule(IRawGrammar &baseGrammar, IRawGrammar &selfGrammar, ExternalReferenceCollector result) {
	if (selfGrammar.patterns) { //&& Array.isArray(context.selfGrammar.patterns)) { [redundant check]
		collectExternalReferencesInRules(
			selfGrammar.patterns,
			baseGrammar,
			selfGrammar,
			selfGrammar.repository,
			result
		);
	}
	if (selfGrammar.injections) {
		array<IRawRule> values = selfGrammar.injections.values();
		collectExternalReferencesInRules(
			values,
			baseGrammar,
			selfGrammar,
			selfGrammar.repository,
			result
		);
	}
}

void collectExternalReferencesInRules(
	array<IRawRule> rules,
	IRawGrammar    &baseGrammar,	/// omitted 'ContextWithRepository' and added the args here
	IRawGrammar    &selfGrammar,
	map<IRawRule> 	repository,
	ExternalReferenceCollector result
) {
	for (const rule of rules) {
		if (result.visitedRule.index_of(rule) >= 0) {
			continue;
		}
		result.visitedRule.push(rule);

		// creates mutable map and i wonder if it needs to be that way
		const patternRepository = rule.repository ? mergeObjects({}, repository, rule.repository) : repository;

		if (rule.patterns) {
			collectExternalReferencesInRules(rule.patterns, baseGrammar, selfGrammar, patternRepository, result);
		}

		const include = rule.include;

		if (!include) {
			continue;
		}

		const reference = parseInclude(include);

		switch (reference.kind) {
			case IncludeReferenceKind.Base:
				collectExternalReferencesInTopLevelRule(baseGrammar, baseGrammar, result); // not a bug.
				break;
			case IncludeReferenceKind.Self:
				collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, result);
				break;
			case IncludeReferenceKind.RelativeReference:
				collectExternalReferencesInTopLevelRepositoryRule(reference.ruleName, baseGrammar, selfGrammar, patternRepository, result);
				break;
			case IncludeReferenceKind.TopLevelReference:
			case IncludeReferenceKind.TopLevelRepositoryReference:
				const selfGrammar =
					reference.scopeName == context.selfGrammar.scopeName
						? context.selfGrammar
						: reference.scopeName == context.baseGrammar.scopeName
						? context.baseGrammar
						: undefined;
				if (selfGrammar) {
					if (reference.kind == IncludeReferenceKind.TopLevelRepositoryReference) {
						collectExternalReferencesInTopLevelRepositoryRule(
							reference.ruleName, baseGrammar, selfGrammar, patternRepository, result);
					} else {
						collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, patternRepository, result);
					}
				} else {
					if (reference.kind == IncludeReferenceKind.TopLevelRepositoryReference) {
						result.add(TopLevelRepositoryRuleReference(reference.scopeName, reference.ruleName));
					} else {
						result.add(TopLevelRuleReference(reference.scopeName));
					}
				}
				break;
		}
	}
};





struct ScopeDependencyProcessor:mx {
	struct members {
		IGrammarRepository repo;
		ScopeName initialScopeName;
		array<ScopeName> seenFullScopeRequests;
		array<str> seenPartialScopeRequests;
		array<TopLevelRuleReference> Q;
	};

	mx_basic(ScopeDependencyProcessor);

	ScopeDependencyProcessor(
		IGrammarRepository repo,
		ScopeName initialScopeName
	) {
		this.seenFullScopeRequests.add(this.initialScopeName);
		this.Q = array<TopLevelRuleReference> { TopLevelRuleReference(this.initialScopeName) };
	}

	void processQueue() {
		const q = this.Q;
		this.Q = {};

		ExternalReferenceCollector deps;
		for (const dep of q) {
			collectReferencesOfReference(dep, this.initialScopeName, this.repo, deps);
		}

		for (auto &dep: deps.references) {
			if (dep instanceof TopLevelRuleReference) {
				if (this.seenFullScopeRequests.has(dep.scopeName)) {
					// already processed
					continue;
				}
				this.seenFullScopeRequests.add(dep.scopeName);
				this.Q.push(dep);
			} else {
				if (this.seenFullScopeRequests.has(dep.scopeName)) {
					// already processed in full
					continue;
				}
				if (this.seenPartialScopeRequests.has(dep.toKey())) {
					// already processed
					continue;
				}
				this.seenPartialScopeRequests.add(dep.toKey());
				this.Q.push(dep);
			}
		}
	}
};

struct OnigString {
	str content;
};

struct IOnigLib {
	virtual OnigScanner createOnigScanner(array<str> sources) = 0;
	virtual OnigString  createOnigString(str string) = 0;
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

/// renamed to IOnigScanner
struct OnigScanner {
	struct members {
		RegEx regex;
	};

	mx_basic(OnigScanner)

	IOnigMatch findNextMatchSync(str string, num startPosition, states<FindOption> options) {
		regex.set_cursor(startPosition);
		return regex.match(string);
	}
	//virtual void dispose() = 0; /// dispose of dispose i think; they only do that because they dont have implicit memory refs with this lib
};

enums(FindOption, None,
	None,
	NotBeginString,
	NotEndString,
	NotBeginPosition,
	DebugCall,
);

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
//	IRawGrammar getExternalGrammar(sr scopeName, IRawRepository repository);
//}

//struct IRuleRegistry IRuleFactoryHelper, IGrammarRegistry {
//}

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
};

struct IRuleRegistry {
	virtual Rule getRule(ruleId: RuleId) = 0;
	virtual Rule registerRule(factory: (id: RuleId) => Rule) = 0;
};

struct IGrammarRegistry {
	getExternalGrammar(scopeName: string, repository: IRawRepository): IRawGrammar | null | undefined;
}

struct IRuleFactoryHelper : IRuleRegistry, IGrammarRegistry { };

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
		console.fault("Not supported!");
	}

	CompiledRule compile(IRuleRegistry grammar, utf16 endRegexSource) {
		console.fault("Not supported!");
	}

	CompiledRule compileAG(IRuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		console.fault("Not supported!");
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

	utf16 get debugMatchRegExp() {
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

	IncludeOnlyRule(ILocation _location, RuleId id, utf16 name, utf16 contentName, ICompilePatternsResult patterns) {
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
	RegExpSource 	_begin;
	CaptureRule 	beginCaptures[];
	RegExpSource 	_end;
	bool 			endHasBackReferences;
	CaptureRule 	endCaptures[];
	bool 			applyEndPatternLast;
	bool 			hasMissingPatterns;
	RuleId 			patterns[];
	RegExpSourceList _cachedCompiledPatterns;

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

	utf16 get debugBeginRegExp() {
		return `${data->_begin.source}`;
	}

	utf16 get debugEndRegExp() {
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
	private RegExpSourceList _cachedCompiledWhilePatterns<RuleId>;

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

	static CaptureRule createCaptureRule(IRuleFactoryHelper helper, ILocation _location, utf16 name, utf16 contentName, RuleId retokenizeCapturedWithRuleId) {
		return helper.registerRule((id) => {
			return new CaptureRule(_location, id, name, contentName, retokenizeCapturedWithRuleId);
		});
	}

	static RuleId getCompiledRuleId(IRawRule desc, IRuleFactoryHelper helper, IRawRepository repository) {
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
					RuleId patterns = desc.patterns;
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

	static CaptureRule _compileCaptures(IRawCaptures captures, IRuleFactoryHelper helper, IRawRepository repository)[] {
		array<CaptureRule> r;

		if (captures) {
			// Find the maximum capture id
			num maximumCaptureId = 0;
			for (auto &captureId: captures) {
				if (captureId == '_vscodeTextmateLocation') {
					continue;
				}
				const numericCaptureId = parseInt(captureId, 10);
				if (numericCaptureId > maximumCaptureId) {
					maximumCaptureId = numericCaptureId;
				}
			}

			// Initialize result
			for (num i = 0; i <= maximumCaptureId; i++) {
				r[i] = null;
			}

			// Fill out result
			for (const captureId in captures) {
				if (captureId == '_vscodeTextmateLocation') {
					continue;
				}
				const numericCaptureId = parseInt(captureId, 10);
				RuleId retokenizeCapturedWithRuleId = 0;
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
							IRawRule &localIncludedRule = repository[reference.ruleName];
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
									auto &externalIncludedRule = externalGrammar.repository[externalGrammarInclude];
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

					bool skipRule = false;

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
			num 			len 		  = regExpSource.len();
			num 			lastPushedPos = 0;
			bool 			hasAnchor 	  = false;
			array<utf16> 	output;
			for (num pos = 0; pos < len; pos++) {
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
		array<str> capturedValues = captureIndices.map<str>(
			[](IOnigCaptureIndex &capture) -> str {
				return lineText.mid(capture.start, capture.end);
			});
		BACK_REFERENCING_END.last_index = 0; // support this!
		return BACK_REFERENCING_END.replace(data->source,[capturedValues] (match, g1) => {
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
						A0_G0_result[pos + 1] = 0xFFFF;
						A0_G1_result[pos + 1] = 0xFFFF;
						A1_G0_result[pos + 1] = 'A';
						A1_G1_result[pos + 1] = 'A';
					} else if (nextCh == 'G') {
						A0_G0_result[pos + 1] = 0xFFFF;
						A0_G1_result[pos + 1] = 'G';
						A1_G0_result[pos + 1] = 0xFFFF;
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

struct IRegExpSourceListAnchorCache {
	CompiledRule A0_G0;
	CompiledRule A0_G1;
	CompiledRule A1_G0;
	CompiledRule A1_G1;
};

struct RegExpSourceList:mx {
	struct members {
		array<RegExpSource> 			_items;
		bool 							_hasAnchors;
		CompiledRule 					_cached;
		IRegExpSourceListAnchorCache 	_anchorCache;
	};

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
		data->_hasAnchors = data->_hasAnchors || item->hasAnchor;
	}

	void unshift(RegExpSource item) {
		data->_items.unshift(item);
		data->_hasAnchors = data->_hasAnchors || item->hasAnchor;
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
			array<utf16> regExps = data->_items.map(
				[](RegExpSource e) -> utf16 {
					return e.source;
				}
			);
			data->_cached = CompiledRule(onigLib, regExps, data->_items.map(
				[](RegExpSource e) -> RuleId {
					return e.ruleId;
				}
			));
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
		auto regExps = data->_items.map<utf16>([](RegExpSource e) -> utf16 {
			return e.resolveAnchors(allowA, allowG);
		});
		return CompiledRule(onigLib, regExps, data->_items.map<RuleId>([](RegExpSource e) -> RuleId {
			return e.ruleId;
		}));
	}
}

struct CompiledRule:mx {
	struct members {
		OnigScanner scanner;
	};

	CompiledRule(IOnigLib onigLib, array<utf16> regExps, array<RuleId> rules) : regExps(regExps), rules(rules) {
		data->scanner = onigLib.createOnigScanner(regExps);
	}

	utf16 toString() {
		array<utf16> r;
		for (num i = 0, len = data->rules.len(); i < len; i++) {
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

struct IFindNextMatchResult {
	RuleId ruleId;
	array<IOnigCaptureIndex> captureIndices;
};

struct StateStack:mx {
	num depth;

	/// implement the same
	virtual StateStack clone() = 0;
	virtual bool equals(StateStack other) = 0;
};

struct StateStackFrame:mx {
	num 	ruleId;
	num 	enterPos;
	num 	anchorPos;
	bool 	beginRuleCapturedEOL;
	utf16 	endRule;
	array<AttributedScopeStackFrame> nameScopesList;
	array<AttributedScopeStackFrame> contentNameScopesList;
}

struct AttributedScopeStack:mx {
	struct members {
		AttributedScopeStack parent;
		ScopeStack scopePath;
		EncodedTokenAttributes tokenAttributes;
	};
	mx_basic(AttributedScopeStack);

	static fromExtension(AttributedScopeStack namesScopeList, array<AttributedScopeStackFrame> contentNameScopesList): AttributedScopeStack {
		AttributedScopeStack current = namesScopeList;
		ScopeStack scopeNames = namesScopeList ?
			namesScopeList.scopePath : {};
		for (const frame of contentNameScopesList) {
			scopeNames = ScopeStack.push(scopeNames, frame.scopeNames);
			current  AttributedScopeStack(current, scopeNames!, frame.encodedTokenAttributes);
		}
		return current;
	}

	static AttributedScopeStackcreateRoot(ScopeName scopeName, EncodedTokenAttributes tokenAttributes) {
		return AttributedScopeStack(null, ScopeStack(null, scopeName), tokenAttributes);
	}

	static AttributedScopeStack createRootAndLookUpScopeName(ScopeName scopeName, EncodedTokenAttributes tokenAttributes, Grammar grammar) {
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

	ScopeName get scopeName() { return data->scopePath.scopeName; }

	public toString() {
		return data->getScopeNames().join(' ');
	}

	bool equals(AttributedScopeStack other) {
		return AttributedScopeStack.equals(this, other);
	}

	static bool equals(
		AttributedScopeStack a,
		AttributedScopeStack b
	) {
		do {
			if (a == b) {
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

			if (a.scopeName != b.scopeName || a.tokenAttributes != b.tokenAttributes) {
				return false;
			}

			// Go to previous pair
			a = a.parent;
			b = b.parent;
		} while (true);
	}

	static EncodedTokenAttributes mergeAttributes(
		EncodedTokenAttributes existingTokenAttributes,
		BasicScopeAttributes basicScopeAttributes,
		StyleAttributes styleAttributes
	) {
		FontStyle fontStyle = FontStyle::NotSet;
		num foreground = 0;
		num background = 0;

		if (styleAttributes != null) {
			fontStyle = styleAttributes.fontStyle;
			foreground = styleAttributes.foregroundId;
			background = styleAttributes.backgroundId;
		}

		return EncodedTokenAttributes::set(
			existingTokenAttributes,
			basicScopeAttributes.languageId,
			basicScopeAttributes.tokenType,
			null,
			fontStyle,
			foreground,
			background
		);
	}

	AttributedScopeStack pushAttributed(ScopePath scopePath, Grammar grammar) {
		if (scopePath == null) {
			return this;
		}

		if (scopePath.indexOf(' ') == -1) {
			// This is the common case and much faster

			return AttributedScopeStack._pushAttributed(this, scopePath, grammar);
		}

		const scopes = scopePath.split(/ /g);
		let AttributedScopeStack result = this;
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
		return data->scopePath.getSegments();
	}

	array<AttributedScopeStackFrame> getExtensionIfDefined(AttributedScopeStack base) {
		const array<AttributedScopeStackFrame> result;
		let AttributedScopeStack self = this;

		while (self && self != base) {
			result.push({
				encodedTokenAttributes: self.tokenAttributes,
				scopeNames: self.scopePath.getExtensionIfDefined(self.parent?.scopePath ?? null)!,
			});
			self = self.parent;
		}
		return self == base ? result.reverse() : undefined;
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

	static bool _equals(StateStackImpl a, StateStackImpl b) {
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
		const array<utf16> r;
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
		return StateStackFrame::members {
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

struct StackDiff {
	num pops;
	array<StateStackFrame> newFrames;
};

StackDiff diffStateStacksRefEq(StateStack first, StateStack second) {
	num pops = 0;
	array<StateStackFrame> newFrames;

	StateStackImpl curFirst  = first;
	StateStackImpl curSecond = second;

	while (curFirst != curSecond) {
		if (curFirst && (!curSecond || curFirst.depth >= curSecond.depth)) {
			// curFirst is certainly not contained in curSecond
			pops++;
			curFirst = curFirst.parent;
		} else {
			// curSecond is certainly not contained in curFirst.
			// Also, curSecond must be defined, as otherwise a previous case would match
			newFrames.push(curSecond!.toStateStackFrame());
			curSecond = curSecond!.parent;
		}
	}
	return {
		pops,
		newFrames.reverse(),
	};
}

StateStackImpl applyStateStackDiff(StateStack stack, StackDiff diff) {
	StateStackImpl curStack = stack;
	for (let i = 0; i < diff.pops; i++) {
		curStack = curStack->parent;
	}
	for (const frame of diff.newFrames) {
		curStack = StateStackImpl.pushFrame(curStack, frame);
	}
	return curStack;
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

struct LineTokens:mx {
	struct members {
		bool 		_emitBinaryTokens;
		utf16 			_lineText;
		array<IToken> 	_tokens;
		array<num> 		_binaryTokens;
		num 			_lastTokenEndIndex;
		array<TokenTypeMatcher> _tokenTypeOverrides;
	};
	
	mx_basic(LineTokens);

	LineTokens(
		bool emitBinaryTokens, utf16 lineText,
		array<TokenTypeMatcher> tokenTypeOverrides,
		array<BalancedBracketSelectors> balancedBracketSelectors)
	{
		data->_emitBinaryTokens = emitBinaryTokens;
		data->_tokenTypeOverrides = tokenTypeOverrides;
		if (DebugFlags.InDebugMode) {
			data->_lineText = lineText;
		} else {
			data->_lineText = null;
		}
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
						metadata = EncodedTokenAttributes::set(
							metadata,
							0,
							toOptionalTokenType(tokenType.type),
							null,
							FontStyle::NotSet,
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
				metadata = EncodedTokenAttributes::set(
					metadata,
					0,
					OptionalStandardTokenType.NotSet,
					containsBalancedBrackets,
					FontStyle::NotSet,
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
				RegEx regex = RegEx(R"(\n$)");
				utf16 txt = data->_lineText.mid(data->_lastTokenEndIndex, endIndex);
				console.log('  token: |' + regex.replace(txt, "\\n") + '|');
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
			RegEx regex = RegEx(R"(\n$)");
			utf16 txt = data->_lineText!.mid(data->_lastTokenEndIndex, endIndex);
			console.log('  token: |' + regex.replace(txt, "\\n") + '|');
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

	array<IToken> getResult(StateStackImpl stack, num lineLength) {
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
	struct members : IGrammar, IRuleFactoryHelper, IOnigLib {
		RuleId 					_rootId = -1;
		num 					_lastRuleId;
		array<Rule> 			_ruleId2desc;
		var 					_grammar;
		array<TokenTypeMatcher> _tokenTypeMatchers;

		OnigScanner createOnigScanner(array<str> sources) {
			return OnigScanner(sources);
		}
		OnigString  createOnigString(str string) = 0;


		//BasicScopeAttributesProvider _basicScopeAttributesProvider;
	};
	mx_basic(Grammar);

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
		if (tokenTypes) {
			array<str> keys = tokenTypes.keys<str>();
			for (str &selector: keys) {
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

	BasicScopeAttributes getMetadataForScope(str scope) {
		return data->_basicScopeAttributesProvider.getBasicScopeAttributes(scope);
	}


	private _collectInjections(): Injection[] {
		const grammarRepository: IGrammarRepository = {
			lookup: (scopeName: string): IRawGrammar | undefined => {
				if (scopeName == this._rootScopeName) {
					return this._grammar;
				}
				return this.getExternalGrammar(scopeName);
			},
			injections: (scopeName: string): string[] => {
				return this._grammarRepository.injections(scopeName);
			},
		};

		const result: Injection[] = [];

		const scopeName = this._rootScopeName;

		const grammar = grammarRepository.lookup(scopeName);
		if (grammar) {
			// add injections from the current grammar
			const rawInjections = grammar.injections;
			if (rawInjections) {
				for (let expression in rawInjections) {
					collectInjections(
						result,
						expression,
						rawInjections[expression],
						this,
						grammar
					);
				}
			}

			// add injection grammars contributed for the current scope

			const injectionScopeNames = this._grammarRepository.injections(scopeName);
			if (injectionScopeNames) {
				injectionScopeNames.forEach((injectionScopeName) => {
					const injectionGrammar =
						this.getExternalGrammar(injectionScopeName);
					if (injectionGrammar) {
						const selector = injectionGrammar.injectionSelector;
						if (selector) {
							collectInjections(
								result,
								selector,
								injectionGrammar,
								this,
								injectionGrammar
							);
						}
					}
				});
			}
		}

		result.sort((i1, i2) => i1.priority - i2.priority); // sort by priority

		return result;
	}

	array<Injection> getInjections() {
		if (this._injections == null) {
			this._injections = this._collectInjections();

			if (DebugFlags.InDebugMode && this._injections.length > 0) {
				console.log(
					"Grammar {0} contains the following injections:"
				, { this._rootScopeName });
				for (Injection &injection: this._injections) {
					console.log("  - {0}", { injection.debugSelector });
				}
			}
		}
		return this._injections;
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
			const defaultMetadata = EncodedTokenAttributes::set(
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


export function createGrammar(
	scopeName: ScopeName,
	grammar: IRawGrammar,
	initialLanguage: number,
	embeddedLanguages: IEmbeddedLanguagesMap | null,
	tokenTypes: ITokenTypeMap | null,
	balancedBracketSelectors: BalancedBracketSelectors | null,
	grammarRepository: 	IGrammarRepository & IThemeProvider,
	onigLib: IOnigLib
): Grammar {
	return new Grammar(
		scopeName,
		grammar,
		initialLanguage,
		embeddedLanguages,
		tokenTypes,
		balancedBracketSelectors,
		grammarRepository,
		onigLib
	); //TODO
}

struct SyncRegistry : mx {
	struct members: IGrammaryRepository, IThemeProvider {
		ion::map<Grammar> _grammars;
		ion::map<IRawGrammar> _rawGrammars;
		ion::map<array<ScopeName>> _injectionGrammars;
		Theme _theme;
	};

	mx_basic(SyncRegistry);

	SyncRegistry(Theme theme):SyncRegistry() {
		data->_theme = theme;
	}

	void setTheme(Theme theme) {
		data->_theme = theme;
	}

	array<str> getColorMap() {
		return data->_theme.getColorMap();
	}

	/**
	 * Add `grammar` to registry and return a list of referenced scope names
	 */
	void addGrammar(IRawGrammar &grammar, array<ScopeName> injectionScopeNames) {
		data->_rawGrammars[grammar.scopeName] = grammar; /// is a copy ok?

		if (injectionScopeNames) {
			data->_injectionGrammars[grammar.scopeName] = injectionScopeNames;
		}
	}

	/**
	 * Lookup a raw grammar.
	 */
	IRawGrammar lookup(ScopeName scopeName) {
		return data->_rawGrammars.get(scopeName)!;
	}

	/**
	 * Returns the injections for the given grammar
	 */
	array<ScopeName> injections(ScopeName targetScope) {
		return data->_injectionGrammars.get(targetScope)!;
	}

	/**
	 * Get the default theme settings
	 */
	StyleAttributes getDefaults() {
		return data->_theme.getDefaults();
	}

	/**
	 * Match a scope in the theme.
	 */
	StyleAttributes themeMatch(ScopeStack scopePath) {
		return data->_theme.match(scopePath);
	}

	/**
	 * Lookup a grammar.
	 */
	future grammarForScopeName(
		ScopeName 					scopeName,
		num 						initialLanguage,
		IEmbeddedLanguagesMap 		embeddedLanguages,
		ITokenTypeMap 				tokenTypes,
		BalancedBracketSelectors 	balancedBracketSelectors
	) {
		if (!data->_grammars.has(scopeName)) {
			let rawGrammar = data->_rawGrammars.get(scopeName)!;
			if (!rawGrammar) {
				return null;
			}
			data->_grammars.set(scopeName, createGrammar(
				scopeName,
				rawGrammar,
				initialLanguage,
				embeddedLanguages,
				tokenTypes,
				balancedBracketSelectors,
				this
			));
		}
		return data->_grammars.get(scopeName)!;
	}
}

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

    mx_basic(Tokenizer);

    Tokenizer(utf16 input) : Tokenizer() {
        data->regex = utf16("([LR]:|[\\w\\.:][\\w\\.:\\-]*|[\\,\\|\\-\\(\\)])");
	    data->match = data->regex.exec(input);
    }
};

using Matcher = lambda<bool(mx)>;

struct MatcherWithPriority:mx {
	struct members {
		Matcher matcher;
		num 	priority; // -1, 0, 1
	};
	mx_basic(MatcherWithPriority);
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

struct Registry:mx {
	struct members {
		RegistryOptions _options;
		SyncRegistry _syncRegistry;
		map<future> _ensureGrammarCache;
	};

	mx_basic(Registry);

	Registry(RegistryOptions options) {
		data->_options = options;
		data->_syncRegistry = SyncRegistry(Theme.createFromRawTheme(options.theme, options.colorMap));
	}

	void dispose() {
		data->_syncRegistry.dispose();
	}

	/**
	 * Change the theme. Once called, no previous `ruleStack` should be used anymore.
	 */
	void setTheme(IRawTheme &theme, array<str> colorMap = {}) {
		data->_syncRegistry.setTheme(Theme.createFromRawTheme(theme, colorMap));
	}

	/**
	 * Returns a lookup array for color ids.
	 */
	array<str> getColorMap() {
		return data->_syncRegistry.getColorMap();
	}

	/**
	 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
	 * Please do not use language id 0.
	 */
	future loadGrammarWithEmbeddedLanguages(
		ScopeName initialScopeName,
		num initialLanguage,
		IEmbeddedLanguagesMap &embeddedLanguages // never pass the structs
	) {
		return data->loadGrammarWithConfiguration(initialScopeName, initialLanguage, { embeddedLanguages });
	}

	/**
	 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
	 * Please do not use language id 0.
	 */
	future loadGrammarWithConfiguration(
		ScopeName initialScopeName,
		num initialLanguage,
		IGrammarConfiguration &configuration
	) {
		return data->_loadGrammar(
			initialScopeName,
			initialLanguage,
			configuration.embeddedLanguages,
			configuration.tokenTypes,
			BalancedBracketSelectors(
				configuration.balancedBracketSelectors,
				configuration.unbalancedBracketSelectors
			)
		);
	}

	/**
	 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
	 */
	future loadGrammar(ScopeName initialScopeName) {
		return data->_loadGrammar(initialScopeName, 0, null, null, null);
	}

	future _loadGrammar(
		ScopeName 					initialScopeName,
		num 						initialLanguage,
		IEmbeddedLanguagesMap 	   &embeddedLanguages,
		ITokenTypeMap 			   &tokenTypes,
		BalancedBracketSelectors 	balancedBracketSelectors
	) {
		const dependencyProcessor = new ScopeDependencyProcessor(data->_syncRegistry, initialScopeName);
		while (dependencyProcessor.Q.length > 0) {
			await Promise.all(dependencyProcessor.Q.map((request) => data->_loadSingleGrammar(request.scopeName)));
			dependencyProcessor.processQueue();
		}

		return data->_grammarForScopeName(
			initialScopeName,
			initialLanguage,
			embeddedLanguages,
			tokenTypes,
			balancedBracketSelectors
		);
	}

	future _loadSingleGrammar(ScopeName scopeName) {
		if (!data->_ensureGrammarCache.has(scopeName)) {
			data->_ensureGrammarCache.set(scopeName, data->_doLoadSingleGrammar(scopeName));
		}
		return data->_ensureGrammarCache.get(scopeName);
	}

	future _doLoadSingleGrammar(ScopeName scopeName) {
		const grammar = await data->_options.loadGrammar(scopeName).then((Grammar g))
		if (grammar) {
			const injections =
				typeof data->_options.getInjections == "function" ? data->_options.getInjections(scopeName) : undefined;
			data->_syncRegistry.addGrammar(grammar, injections);
		}
	}

	/**
	 * Adds a rawGrammar.
	 */
	future addGrammar(
		IRawGrammar &rawGrammar,
		array<str> injections = {},
		num initialLanguage = 0,
		IEmbeddedLanguagesMap embeddedLanguages = null)
	{
		data->_syncRegistry.addGrammar(rawGrammar, injections);
		return (await data->_grammarForScopeName(rawGrammar.scopeName, initialLanguage, embeddedLanguages))!;
	}

	/**
	 * Get the grammar for `scopeName`. The grammar must first be created via `loadGrammar` or `addGrammar`.
	 */
	future _grammarForScopeName(
		str scopeName,
		num initialLanguage = 0,
		IEmbeddedLanguagesMap embeddedLanguages = null,
		ITokenTypeMap tokenTypes = null,
		BalancedBracketSelectors balancedBracketSelectors = null
	) {
		return data->_syncRegistry.grammarForScopeName(
			scopeName,
			initialLanguage,
			embeddedLanguages,
			tokenTypes,
			balancedBracketSelectors
		);
	}
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
