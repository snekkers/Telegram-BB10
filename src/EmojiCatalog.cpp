#include "EmojiCatalog.hpp"
#include "PrivateFile.hpp"
EmojiCatalog::EmojiCatalog(QObject *parent):QObject(parent),m_entries(new bb::cascades::ArrayDataModel(this)),m_page(0){QVariantMap saved;if(PrivateFile::read("data/settings/recent-emoji.json",&saved,8192) || PrivateFile::read("data/recent-emoji.json",&saved,8192))m_recent=saved.value("symbols").toStringList();while(m_recent.size()>30)m_recent.removeLast();}
QString EmojiCatalog::position() const{return QString("%1 / %2").arg(m_symbols.isEmpty()?0:m_page+1).arg((m_symbols.size()+29)/30);}
void EmojiCatalog::setCategory(int index){
    static const char *groups[]={
        "😀 😃 😄 😁 😆 😅 😂 🤣 😊 😇 🙂 🙃 😉 😌 😍 🥰 😘 😗 😙 😚 😋 😛 😝 😜 🤪 🤨 🧐 🤓 😎 🥳 😏 😒 😞 😔 😟 😕 🙁 ☹️ 😣 😖 😫 😩 🥺 😢 😭 😤 😠 😡 🤬 🤯 😳 🥵 🥶 😱 😨 😰 😥 😓 🤗 🤔 🫡 🤭 🫢 🫣 🤫 🤥 😶 🫠 😐 😑 🫤 😬 🙄 😯 😦 😧 😮 😲 🥱 😴 🤤 😪 😵 🤐 🥴 🤢 🤮 🤧 😷 🤒 🤕 🤑 🤠 😈 👿 👻 💀 👽 🤖 💩",
        "👍 👍🏻 👍🏼 👍🏽 👍🏾 👍🏿 👎 👏 🙌 👐 🤲 🤝 🙏 ✌️ 🤞 🤟 🤘 👌 🤌 🤏 👈 👉 👆 👇 ☝️ ✋ 🤚 🖐️ 🖖 👋 🤙 💪 🦾 ✍️ 💅 👀 🧠 🫶 ❤️ 🧡 💛 💚 💙 💜 🖤 🤍 🤎 💔 ❤️‍🔥 💕 💞 💓 💗 💖 💘 💝 💟 💯 💢 💥 💫 💦 💤",
        "🐶 🐱 🐭 🐹 🐰 🦊 🐻 🐼 🐻‍❄️ 🐨 🐯 🦁 🐮 🐷 🐸 🐵 🙈 🙉 🙊 🐔 🐧 🐦 🐤 🦆 🦉 🦋 🐝 🐞 🐢 🐬 🐳 🦈 🐙 🌸 🌺 🌻 🌹 🌷 🌱 🌲 🌳 🌴 🌵 🍀 🍁 🍂 🍃 ☀️ 🌤️ ☁️ 🌧️ ⛈️ 🌈 ❄️ 🔥 ⭐ 🌟 🌙 🌍",
        "🍎 🍏 🍐 🍊 🍋 🍌 🍉 🍇 🍓 🫐 🍒 🍑 🥭 🍍 🥝 🍅 🥑 🥦 🥒 🌶️ 🥕 🌽 🥔 🥐 🍞 🧀 🍳 🥓 🥩 🍗 🍔 🍟 🍕 🌭 🥪 🌮 🌯 🥗 🍝 🍜 🍣 🍤 🍦 🍩 🍪 🎂 🍰 🍫 🍬 🍭 ☕ 🍵 🧃 🥤 🍺 🍷 🥂 🍽️",
        "🎉 🎊 🎈 🎁 🎂 🎄 🎃 ⚽ 🏀 🎾 🏆 🎮 🎧 🎤 🎬 🎨 🎯 🚗 🚕 🚌 🚲 🏍️ ✈️ 🚀 🚁 🚂 ⛵ 🏠 🏢 🏖️ 🏔️ 🗺️ 📱 💻 ⌨️ 🖥️ 📷 💡 🔦 🕯️ 🔑 🔒 🔓 📚 📖 📝 ✏️ 📎 📌 📍 ✅ ❌ ❓ ❗ ⚠️ 🔴 🟠 🟡 🟢 🔵 🟣 ⚪ ⚫ 0️⃣ 1️⃣ 2️⃣ 3️⃣ 4️⃣ 5️⃣ 6️⃣ 7️⃣ 8️⃣ 9️⃣ 🔟",
        "🇷🇺 🇧🇾 🇰🇿 🇺🇦 🇬🇪 🇦🇲 🇦🇿 🇺🇿 🇰🇬 🇹🇯 🇹🇷 🇨🇳 🇯🇵 🇰🇷 🇮🇳 🇦🇪 🇮🇱 🇪🇬 🇿🇦 🇺🇸 🇨🇦 🇲🇽 🇧🇷 🇦🇷 🇬🇧 🇩🇪 🇫🇷 🇮🇹 🇪🇸 🇵🇹 🇳🇱 🇧🇪 🇨🇭 🇦🇹 🇸🇪 🇳🇴 🇫🇮 🇩🇰 🇵🇱 🇨🇿 🇬🇷 🇷🇸 🇦🇺 🇳🇿 🏳️‍🌈 🏁 🚩 🏴‍☠️"};
    m_page=0;if(index==0)m_symbols=m_recent;else m_symbols=QString::fromUtf8(groups[qBound(0,index-1,5)]).split(' ',QString::SkipEmptyParts);rebuild();
}
void EmojiCatalog::rebuild(){m_entries->clear();for(int i=m_page*30;i<qMin((m_page+1)*30,m_symbols.size());++i){QVariantMap row;row.insert("symbol",m_symbols.at(i));m_entries->append(QVariant(row));}emit changed();}
void EmojiCatalog::movePage(int delta){if(delta>0 && canNext())++m_page;else if(delta<0 && canPrevious())--m_page;else return;rebuild();}
void EmojiCatalog::remember(const QString &symbol){if(symbol.isEmpty() || symbol.size()>64)return;m_recent.removeAll(symbol);m_recent.prepend(symbol);while(m_recent.size()>30)m_recent.removeLast();QVariantMap saved;saved.insert("symbols",m_recent);PrivateFile::write("data/settings/recent-emoji.json",saved,8192);}
