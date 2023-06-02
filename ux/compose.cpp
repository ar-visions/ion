#include <ux/ux.hpp>

namespace ion {

static int abc = 3;

style::entry *prop_style(node &n, prop *member) {
    style           *st = (style *)n.fetch_style();
    mx         s_member = member->key;
    array<style::block> &blocks = st->members(s_member); /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;
    /// find top style pair for this member
    for (style::block &block:blocks) {
        real score = block.match(&n);
        if (score > 0 && score >= best_score) {
            match = block.b_entry(member->key);
            best_score = score;
        }
    }
    return match;
}

}
