#ifndef BERRYGRAM_PREFERENCES_HPP
#define BERRYGRAM_PREFERENCES_HPP
#include <QObject>
#include <QVariantMap>
class Preferences:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString canvas READ canvas NOTIFY changed)
    Q_PROPERTY(QString ink READ ink NOTIFY changed)
    Q_PROPERTY(QString secondary READ secondary NOTIFY changed)
    Q_PROPERTY(bool hidePreview READ hidePreview WRITE setHidePreview NOTIFY changed)
    Q_PROPERTY(bool sendOnEnter READ sendOnEnter WRITE setSendOnEnter NOTIFY changed)
    Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY changed)
    Q_PROPERTY(int photoCacheMb READ photoCacheMb WRITE setPhotoCacheMb NOTIFY changed)
public:
    explicit Preferences(QObject *parent=0);
    QString error() const{return m_error;}
    bool dark() const{return m_values.value("dark",false).toBool();}
    QString surface() const{return dark()?"#202a33":"#ffffff";}
    QString canvas() const{return dark()?"#101820":"#e8eff2";}
    QString ink() const{return dark()?"#f0f4f6":"#202b33";}
    QString secondary() const{return dark()?"#bac7d0":"#75838e";}
    void setDark(bool v){set("dark",v);}
    bool hidePreview() const{return m_values.value("hidePreview",false).toBool();}
    bool sendOnEnter() const{return m_values.value("sendOnEnter",true).toBool();}
    int textSize() const{return m_values.value("textSize",1).toInt();}
    int photoCacheMb() const{return m_values.value("photoCacheMb",24).toInt();}
    void setHidePreview(bool v){set("hidePreview",v);}void setSendOnEnter(bool v){set("sendOnEnter",v);}
    void setTextSize(int v){set("textSize",qBound(0,v,2));}
    void setPhotoCacheMb(int v){if(v==8||v==24||v==64)set("photoCacheMb",v);}
signals:void changed();
private:void applyTheme();void set(const QString &key,const QVariant &value);QVariantMap m_values;QString m_error;
};
#endif
