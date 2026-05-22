#include "messageviewerplugin.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QDebug>
#include <QDateTime>
#include <QLineEdit>
#include <QJsonArray>

MessageViewerPlugin::MessageViewerPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_widget(nullptr)
    , m_messageList(nullptr)
    , m_detailView(nullptr)
    , m_topicList(nullptr)
    , m_maxMessages(100)
{
    m_info.id = "message_viewer";
    m_info.name = tr("Message Viewer");
    m_info.version = "0.1";
    m_info.description = tr("Screen plugin for displaying all messages from the plugin message bus");
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Screen;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "View Messages" << "Filter Messages";
    m_info.subscribeTopics = QStringList() << ">";
    m_info.publishTopics = QStringList();
}

MessageViewerPlugin::~MessageViewerPlugin()
{
    unload();
}

PluginInfo MessageViewerPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> MessageViewerPlugin::getMenuEntries() const
{
    QVector<MenuEntry> entries;
    MenuEntry entry;
    entry.topMenu = tr("View");
    entry.subMenus = QStringList() << tr("Show Message Viewer") << tr("Clear Messages");
    entries.append(entry);
    return entries;
}

bool MessageViewerPlugin::load()
{
    qDebug() << "Message Viewer Plugin: Loading";
    return true;
}

bool MessageViewerPlugin::initialize()
{
    qDebug() << "Message Viewer Plugin: Initializing";

    m_widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_widget);

    QLabel* titleLabel = new QLabel(tr("NATS Messages"), m_widget);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14pt;");
    layout->addWidget(titleLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* clearBtn = new QPushButton(tr("Clear"), m_widget);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    connect(clearBtn, &QPushButton::clicked, this, &MessageViewerPlugin::clearMessages);

    QLabel* topicLabel = new QLabel(tr("Topic Subscriptions:"), m_widget);
    layout->addWidget(topicLabel);

    QHBoxLayout* topicLayout = new QHBoxLayout();
    QLineEdit* topicInput = new QLineEdit(m_widget);
    topicInput->setPlaceholderText(tr("Enter topic (e.g., >, mil.*, mil.air.*)"));
    QPushButton* addTopicBtn = new QPushButton(tr("Add"), m_widget);
    QPushButton* removeTopicBtn = new QPushButton(tr("Remove"), m_widget);
    topicLayout->addWidget(topicInput);
    topicLayout->addWidget(addTopicBtn);
    topicLayout->addWidget(removeTopicBtn);
    layout->addLayout(topicLayout);

    m_topicList = new QListWidget(m_widget);
    m_topicList->setMaximumHeight(80);
    for (const QString& topic : m_info.subscribeTopics) {
        m_topicList->addItem(topic);
    }
    layout->addWidget(m_topicList);

    connect(addTopicBtn, &QPushButton::clicked, this, [this, topicInput]() {
        QString topic = topicInput->text().trimmed();
        if (!topic.isEmpty() && !m_info.subscribeTopics.contains(topic)) {
            m_info.subscribeTopics.append(topic);
            m_topicList->addItem(topic);
            topicInput->clear();
            qDebug() << "Message Viewer Plugin: Added topic subscription:" << topic;
        }
    });

    connect(removeTopicBtn, &QPushButton::clicked, this, &MessageViewerPlugin::removeTopicSubscription);

    QSplitter* splitter = new QSplitter(Qt::Vertical, m_widget);

    m_messageList = new QListWidget(m_widget);
    m_messageList->setMaximumHeight(150);
    splitter->addWidget(m_messageList);

    m_detailView = new QTextEdit(m_widget);
    m_detailView->setReadOnly(true);
    m_detailView->setPlaceholderText(tr("Select a message to view details..."));
    splitter->addWidget(m_detailView);

    splitter->setSizes(QList<int>() << 150 << 200);

    layout->addWidget(splitter);

    connect(m_messageList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && m_messageList->item(row)) {
            QString detail = m_messageList->item(row)->data(Qt::UserRole).toString();
            m_detailView->setPlainText(detail);
        }
    });

    return true;
}

bool MessageViewerPlugin::start()
{
    qDebug() << "Message Viewer Plugin: Starting";
    return true;
}

bool MessageViewerPlugin::stop()
{
    qDebug() << "Message Viewer Plugin: Stopping";
    return true;
}

bool MessageViewerPlugin::unload()
{
    qDebug() << "Message Viewer Plugin: Unloading";
    delete m_widget;
    m_widget = nullptr;
    return true;
}

QWidget* MessageViewerPlugin::getWidget()
{
    return m_widget;
}

void MessageViewerPlugin::configure(QWidget* parent)
{
}

void MessageViewerPlugin::deliverMessage(const QString& topic, const QString& payload)
{
    if (!m_messageList) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, topic, payload]() {
        QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString display = QString("[%1] %2: %3").arg(time).arg(topic).arg(payload.left(50));

        m_messageList->addItem(display);

        QString detail = QString("Topic: %1\nTimestamp: %2\n\nPayload:\n%3")
            .arg(topic)
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
            .arg(payload);

        m_messageList->item(m_messageList->count() - 1)->setData(Qt::UserRole, detail);

        if (m_messageList->count() > m_maxMessages) {
            delete m_messageList->takeItem(0);
        }
    }, Qt::QueuedConnection);
}

void MessageViewerPlugin::clearMessages()
{
    m_messageList->clear();
    m_detailView->clear();
}

void MessageViewerPlugin::addTopicSubscription()
{
}

void MessageViewerPlugin::removeTopicSubscription()
{
    int row = m_topicList->currentRow();
    if (row >= 0) {
        QListWidgetItem* item = m_topicList->takeItem(row);
        QString topic = item->text();
        delete item;
        m_info.subscribeTopics.removeAll(topic);
        qDebug() << "Message Viewer Plugin: Removed topic subscription:" << topic;
    }
}

QJsonObject MessageViewerPlugin::getConfig() const
{
    QJsonObject config;
    config["maxMessages"] = m_maxMessages;
    QJsonArray topicsArray;
    for (const QString& topic : m_info.subscribeTopics) {
        topicsArray.append(topic);
    }
    config["subscribeTopics"] = topicsArray;
    return config;
}

void MessageViewerPlugin::setConfig(const QJsonObject& config)
{
    m_maxMessages = config.value("maxMessages").toInt(100);
    if (config.contains("subscribeTopics")) {
        QJsonArray topicsArray = config.value("subscribeTopics").toArray();
        m_info.subscribeTopics.clear();
        if (m_topicList) {
            m_topicList->clear();
        }
        for (const QJsonValue& value : topicsArray) {
            QString topic = value.toString();
            m_info.subscribeTopics.append(topic);
            if (m_topicList) {
                m_topicList->addItem(topic);
            }
        }
    }
}