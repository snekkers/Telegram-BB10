#include "Preferences.hpp"
#include "PrivateFile.hpp"
#include <bb/cascades/Application>
#include <bb/cascades/ThemeSupport>
#include <bb/cascades/VisualStyle>
#include <bb/cascades/Color>
Preferences::Preferences(QObject *parent):QObject(parent){PrivateFile::read("data/settings/preferences.json",&m_values,8192);applyTheme();}
void Preferences::set(const QString &key,const QVariant &value){if(m_values.value(key)==value)return;QVariantMap next=m_values;next.insert(key,value);if(PrivateFile::write("data/settings/preferences.json",next,8192)){m_values=next;m_error.clear();if(key=="dark")applyTheme();}else m_error=QString::fromUtf8("Не удалось сохранить настройки.");emit changed();}

void Preferences::applyTheme(){bb::cascades::Application::instance()->themeSupport()->setVisualStyleAndPrimaryColor(dark()?bb::cascades::VisualStyle::Dark:bb::cascades::VisualStyle::Bright,bb::cascades::Color::fromARGB(0xff5288b7),bb::cascades::Color::fromARGB(0xff42739d));}
