#ifndef COTMESSAGE_H
#define COTMESSAGE_H

#include <QString>
#include <QDateTime>

enum class CotEventType {
    a_f_G_E_V_C,
    a_u_G_F_I,
    b_m_p_s_p,
    t_x_c_o_n,
    i_g_o_r_f,
    i_g_o_e_m,
    i_g_o_e_q,
    i_g_o_f_r,
    i_g_o_s_a,
    i_g_o_u_a,
    i_g_o_w_p,
    i_g_o_a_c,
    i_g_o_p_r,
    i_g_o_p_o,
    i_g_o_l_n,
    i_g_o_l_x,
    i_g_o_d_p,
    i_g_o_f_s,
    i_g_o_c_k,
    i_g_o_s_p,
    i_g_o_i_c,
    i_g_o_m_i,
    i_g_o_e_v,
    i_g_o_s_e,
    i_g_o_c_r,
    i_g_o_c_e,
    i_g_o_d_c,
    i_g_o_d_d,
    i_g_o_d_i,
    i_g_o_d_l,
    i_g_o_d_m,
    i_g_o_d_s,
    i_g_o_d_t,
    i_g_o_d_u,
    i_g_o_d_v,
    i_g_o_d_w,
    i_g_o_d_x,
    i_g_o_d_y,
    i_g_o_d_z,
    unknown
};

struct CoTPoint {
    double lat = 0.0;
    double lon = 0.0;
    double hae = 0.0;
    double ce = 0.0;
    double le = 0.0;
};

struct CoTContact {
    QString callsign;
    QString endpoint;
};

struct CoTDetail {
    QString type;
    QString how;
    CoTContact contact;
    CoTPoint point;
    QString uid;
    QString remarks;
};

struct CoTMessage {
    QString uid;
    CotEventType eventType;
    QString how;
    QDateTime time;
    QDateTime start;
    QDateTime stale;
    CoTPoint point;
    CoTContact contact;
    QString remarks;
    QString rawXml;
};

class CoTMessageBuilder {
public:
    static QString buildPositionReport(
        const QString& uid,
        const QString& callsign,
        const QString& type,
        const QString& how,
        double lat,
        double lon,
        double hae,
        double ce,
        double le,
        const QString& groupName = QString(),
        const QString& role = QString(),
        const QString& takvDevice = QString(),
        const QString& takvOs = QString(),
        const QString& takvPlatform = QString(),
        const QString& takvVersion = QString(),
        const QString& remarks = QString()
    );

    static QString buildChatMessage(
        const QString& uid,
        const QString& callsign,
        const QString& chatGroup,
        const QString& message,
        const QString& toUid = QString()
    );
};

class CoTMessageParser {
public:
    static CoTMessage parse(const QString& xml);
    static bool isValid(const QString& xml);
};

#endif
