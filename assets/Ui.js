.pragma library

var blue = "#5288b7";
var ink = "#202b33";
var secondary = "#75838e";
var canvas = "#e8eff2";

function initials(title) {
    var words = String(title || "?").trim().split(/\s+/);
    var first = words[0];
    // Keep an emoji surrogate pair intact.
    var count = first.charCodeAt(0) >= 0xd800 && first.charCodeAt(0) <= 0xdbff ? 2 : 1;
    return (first.substr(0, count) + (words.length > 1 && count === 1 ? words[words.length - 1].charAt(0) : "")).toUpperCase();
}

function avatar(id) {
    var hash = 0, value = String(id || "");
    for (var i = 0; i < value.length; ++i) hash = (hash * 31 + value.charCodeAt(i)) & 0x7fffffff;
    return "asset:///ui/avatar-" + (hash % 6) + ".png";
}
