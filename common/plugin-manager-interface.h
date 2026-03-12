#ifndef BASEMANAGER_H
#define BASEMANAGER_H
#include <QObject>
namespace UkuiSettingsDaemon {
class ManagerInterface;
}

class ManagerInterface: public QObject
{
    Q_OBJECT
public:
    virtual ~ManagerInterface() {}

    virtual bool Start () = 0;
    virtual void Stop () = 0;
};
#endif // BASEMANAGER_H
