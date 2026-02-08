#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTouchEvent> 
#include <QScreen>
#include <QTimer>
#include <QSocketNotifier>
#include <QtMath>
#include <QDateTime>
#include <QRegion>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <signal.h> 

extern "C" {
    #include <xdo.h>
    #include <X11/Xlib.h> // We need raw Xlib for Transient hints
}

// Configuration
struct RadialButton {
    QString label;
    std::string key;
    double rInner; 
    double rOuter; 
    double startAngle; 
    double spanAngle;  
    QColor baseColor;
    bool isPressed = false;
    bool isCenter = false;
};

class RadialOverlay : public QWidget {
    Q_OBJECT
public:
    xdo_t* xdo;
    Window gameWindowID = 0;
    int gamePid = 0; 
    QTimer *focusTimer;
    QTimer *repeatTimer;
    QTimer *holdTimer;
    
    std::vector<RadialButton> buttons;
    int hoveredIndex = -1;
    int pressedIndex = -1;
    float holdProgress = 0.0f; 
    qint64 lastCloseTime = 0;

    // Dragging State
    QPoint dragStartPosition;
    QPoint windowStartPosition;
    bool isDragging = false;

    RadialOverlay(QWidget *parent = nullptr) : QWidget(parent) {
        // --- WINDOW FLAGS ---
        // We keep 'WindowDoesNotAcceptFocus' so we don't minimize the game when clicked.
        // We rely on 'X11BypassWindowManagerHint' to float above.
        setWindowFlags(Qt::FramelessWindowHint | 
                       Qt::WindowStaysOnTopHint | 
                       Qt::Tool | 
                       Qt::X11BypassWindowManagerHint |
                       Qt::WindowDoesNotAcceptFocus);
                       
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_AcceptTouchEvents); 
        
        QScreen *screen = QGuiApplication::primaryScreen();
        int size = std::min(screen->geometry().width(), screen->geometry().height()) * 0.35; 
        setFixedSize(size, size);

        QColor cMove = QColor(0, 128, 128, 200);   
        QColor cAction = QColor(128, 0, 128, 200); 
        QColor cClose = QColor(160, 80, 80, 220);  

        buttons.push_back({"X", "", 0.0, 0.25, 0, 360, cClose, false, true});
        buttons.push_back({"D", "d", 0.25, 0.60, -90, 180, cMove});
        buttons.push_back({"A", "a", 0.25, 0.60,  90, 180, cMove});
        buttons.push_back({"→", "Right", 0.60, 1.0, -45, 90, cAction}); 
        buttons.push_back({"↑", "Up",    0.60, 1.0,  45, 90, cAction}); 
        buttons.push_back({"←", "Left",  0.60, 1.0, 135, 90, cAction}); 
        buttons.push_back({"↓", "Down",  0.60, 1.0, 225, 90, cAction}); 

        xdo = xdo_new(NULL);
        setupIpc();

        focusTimer = new QTimer(this);
        connect(focusTimer, &QTimer::timeout, this, &RadialOverlay::checkFocus);
        focusTimer->start(200); 

        repeatTimer = new QTimer(this);
        connect(repeatTimer, &QTimer::timeout, this, [this](){
            if (pressedIndex != -1 && !buttons[pressedIndex].isCenter) {
                sendKeyToGame(buttons[pressedIndex].key.c_str());
            }
        });

        holdTimer = new QTimer(this);
        connect(holdTimer, &QTimer::timeout, this, [this](){
            if (isDragging) return; 
            holdProgress += 0.05f; 
            if (holdProgress >= 1.0f) {
                holdTimer->stop();
                closeOverlay();
            }
            update();
        });
        
        updateMask();
    }

    void updateMask() {
        QPainterPath circlePath;
        qreal cx = width() / 2.0;
        qreal cy = height() / 2.0;
        qreal r = std::min(cx, cy) - 1.0;
        circlePath.addEllipse(cx - r, cy - r, r * 2, r * 2);
        setMask(QRegion(circlePath.toFillPolygon().toPolygon()));
    }

    void closeOverlay() {
        lastCloseTime = QDateTime::currentMSecsSinceEpoch();
        this->hide();
        holdProgress = 0.0f;
    }

    // --- INPUT HANDLING ---
    void handlePress(const QPointF &localPos, const QPointF &globalPos) {
        int idx = getButtonIndexAt(localPos); 
        if (idx != -1) {
            pressedIndex = idx;
            buttons[idx].isPressed = true;
            if (buttons[idx].isCenter) {
                holdProgress = 0.0f;
                holdTimer->start(50);
                dragStartPosition = globalPos.toPoint();
                windowStartPosition = this->pos();
                isDragging = false;
            } else {
                sendKeyToGame(buttons[idx].key.c_str());
                repeatTimer->start(10); 
            }
            update(); 
        }
    }

    void handleRelease(const QPointF &pos) {
        if (pressedIndex != -1) {
            holdTimer->stop();
            repeatTimer->stop();
            buttons[pressedIndex].isPressed = false;
            pressedIndex = -1;
            holdProgress = 0.0f;
            isDragging = false;
            update();
        }
    }

    void handleMove(const QPointF &localPos, const QPointF &globalPos) {
        if (pressedIndex != -1 && buttons[pressedIndex].isCenter) {
            if (!isDragging) {
                if ((globalPos.toPoint() - dragStartPosition).manhattanLength() > 10) {
                    isDragging = true;
                    holdProgress = 0.0f; 
                    holdTimer->stop();   
                    update(); 
                }
            }
            if (isDragging) {
                QPoint delta = globalPos.toPoint() - dragStartPosition;
                this->move(windowStartPosition + delta);
                return; 
            }
        }
        
        int idx = getButtonIndexAt(localPos);
        if (pressedIndex != -1 && buttons[pressedIndex].isCenter && !isDragging && idx != pressedIndex) {
            holdTimer->stop();
            holdProgress = 0.0f;
            update();
        }
        if (idx != hoveredIndex) {
            hoveredIndex = idx;
            update();
        }
    }

    // --- OVERRIDES ---
    void mousePressEvent(QMouseEvent *e) override { handlePress(e->position(), e->globalPosition()); }
    void mouseReleaseEvent(QMouseEvent *e) override { handleRelease(e->position()); }
    void mouseMoveEvent(QMouseEvent *e) override { handleMove(e->position(), e->globalPosition()); }

    bool event(QEvent *e) override {
        if (e->type() == QEvent::TouchBegin) {
            QTouchEvent *touch = static_cast<QTouchEvent*>(e);
            if (!touch->points().isEmpty()) {
                handlePress(touch->points().first().position(), touch->points().first().globalPosition());
                return true;
            }
        }
        else if (e->type() == QEvent::TouchUpdate) {
            QTouchEvent *touch = static_cast<QTouchEvent*>(e);
            if (!touch->points().isEmpty()) {
                handleMove(touch->points().first().position(), touch->points().first().globalPosition());
                return true;
            }
        }
        else if (e->type() == QEvent::TouchEnd) {
            QTouchEvent *touch = static_cast<QTouchEvent*>(e);
            if (!touch->points().isEmpty()) {
                handleRelease(touch->points().first().position());
                return true;
            }
        }
        return QWidget::event(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        qreal cx = width() / 2.0;
        qreal cy = height() / 2.0;
        qreal radius = std::min(cx, cy) - 1.0; 

        for (int i = 0; i < (int)buttons.size(); ++i) {
            const auto &btn = buttons[i];
            QPainterPath path;
            qreal rIn = btn.rInner * radius;
            qreal rOut = btn.rOuter * radius + 0.5;
            
            path.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, btn.startAngle);
            path.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, btn.startAngle, btn.spanAngle);
            path.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, btn.startAngle + btn.spanAngle, -btn.spanAngle);
            path.closeSubpath();

            QColor fill = btn.baseColor;
            if (btn.isPressed) fill = fill.lighter(140);
            else if (i == hoveredIndex) fill = fill.lighter(115);

            p.setPen(QPen(fill, 1.2)); 
            p.setBrush(fill);
            p.drawPath(path);

            if (btn.isCenter && holdProgress > 0 && !isDragging) {
                p.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                p.drawArc(QRectF(cx - rIn, cy - rIn, rIn*2, rIn*2), 90 * 16, -holdProgress * 360 * 16);
            }

            p.setPen(Qt::white);
            QFont f = font();
            f.setBold(true);
            f.setPixelSize(width() * (btn.isCenter ? 0.09 : 0.07));
            p.setFont(f);
            
            qreal tx = cx, ty = cy;
            if (!btn.isCenter) {
                qreal midAngle = qDegreesToRadians(btn.startAngle + (btn.spanAngle / 2.0));
                qreal midR = (rIn + rOut) / 2.0;
                tx = cx + midR * std::cos(-midAngle);
                ty = cy + midR * std::sin(-midAngle);
            }
            p.drawText(QRectF(tx - 60, ty - 60, 120, 120), Qt::AlignCenter, btn.label);
        }
    }

    int getButtonIndexAt(const QPointF &p) {
        qreal cx = width()/2.0;
        qreal cy = height()/2.0;
        qreal dx = p.x() - cx;
        qreal dy = p.y() - cy;
        double distPct = std::sqrt(dx*dx + dy*dy) / (std::min(cx, cy));
        
        if (distPct > 1.0) return -1;
        double angle = qRadiansToDegrees(std::atan2(-dy, dx));
        if (angle < 0) angle += 360;

        for (int i = 0; i < (int)buttons.size(); ++i) {
            if (distPct >= buttons[i].rInner && distPct <= buttons[i].rOuter) {
                double norm = angle;
                while (norm < buttons[i].startAngle) norm += 360;
                while (norm >= buttons[i].startAngle + 360) norm -= 360;
                if (norm >= buttons[i].startAngle && norm < buttons[i].startAngle + buttons[i].spanAngle) return i;
            }
        }
        return -1;
    }

    // --- AGGRESSIVE FOCUS & Z-ORDER CHECK ---
    void checkFocus() {
        if (gamePid > 0) {
            if (kill(gamePid, 0) == -1) {
                qApp->quit(); 
                return;
            }
        }

        if (!isVisible()) return;

        // Force overlay to top of stack every cycle
        this->raise(); 

        Window active; 
        xdo_get_active_window(xdo, &active);

        // If active is 0, we might be in a transient state (e.g. clicking the overlay), so don't close.
        if (active == 0) return;

        if (active != gameWindowID && active != this->winId()) {
            closeOverlay();
        }
    }

    void sendKeyToGame(const char* key) {
        if (gameWindowID != 0) xdo_send_keysequence_window(xdo, gameWindowID, key, 0);
    }

    void setupIpc() {
        int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        unlink("/tmp/game_overlay_socket");
        strncpy(addr.sun_path, "/tmp/game_overlay_socket", sizeof(addr.sun_path)-1);
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) return;

        auto notifier = new QSocketNotifier(sock, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this, sock](){
            char buf[64];
            ssize_t len = recv(sock, buf, sizeof(buf)-1, 0);
            if (len > 0) { buf[len] = '\0'; processCommand(QString::fromUtf8(buf)); }
        });
    }

    void processCommand(const QString &cmd) {
        if (isVisible() || !cmd.startsWith("SHOW")) return;
        if (QDateTime::currentMSecsSinceEpoch() - lastCloseTime < 1000) return;

        int rx, ry;
        if (sscanf(cmd.toStdString().c_str() + 5, "%d %d", &rx, &ry) == 2) {
            xdo_get_active_window(xdo, &gameWindowID);
            
            if (gameWindowID != 0) {
                if (gamePid == 0) gamePid = xdo_get_pid_window(xdo, gameWindowID);
                
                // --- X11 MAGIC: Attach Overlay to Game ---
                Display *dpy = QGuiApplication::platformNativeInterface()->nativeResourceForWindow("display", NULL);
                if (dpy) {
                    XSetTransientForHint((Display*)dpy, this->winId(), gameWindowID);
                }
            }

            this->move(rx / devicePixelRatio() - width()/2, ry / devicePixelRatio() - height()/2);
            this->show();
            this->raise();
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents);
    qputenv("QT_QPA_PLATFORM", "xcb");
    QApplication app(argc, argv);
    app.setApplicationName("GameOverlay");
    app.setDesktopFileName("GameOverlay"); 
    RadialOverlay w;
    return app.exec();
}