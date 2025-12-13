#include <QObject>
#include <QPolygonF>

class MissionPlanner : public QObject {
    Q_OBJECT
public:
    // Called from QML when user draws a polygon on the map
    Q_INVOKABLE void add_no_fire_zone(const QPolygonF& zone_points);

    // Called by CoreClient to serialize and send to the Core
    QByteArray get_geofence_data() const;
private:
    std::vector<QPolygonF> no_fire_zones_;
};