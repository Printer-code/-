#ifndef JUST_TEST_H
#define JUST_TEST_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>
#include <QPushButton>

// 游戏状态枚举
enum GameState
{
    TITLE,
    PLAYING,
    PAUSED,
    GAMEOVER,
    TUTORIAL
};
enum PlayerAction
{
    RUNNING,
    JUMPING,
    SLIDING
};
enum ObstacleType
{
    JUMP_OVER,
    SLIDE_UNDER,
    DODGE_ONLY
};

class JustTestGame : public QGraphicsView
{
    Q_OBJECT

public:
    JustTestGame(QWidget *parent = nullptr);
    ~JustTestGame();

private slots:
    void gameLoop();
    void spawnEntities();
    void keyPressEvent(QKeyEvent *event) override;
    void resetPlayerAction();
    void endInvincible();
    void clearEffect();
    void setGameState(GameState state);
    void onRestartClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onQuitClicked();
    void onToggleVolumeClicked();
    void updatePerspectiveScale(QGraphicsPixmapItem *item);

    // 教程槽函数
    void onTutorialClicked();
    void closeTutorial();

private:
    void initAudio();
    void initUI();
    void resetGame();
    void clearEntities(); // [新增] 清理屏幕上的道具、障碍物和云朵

    // 场景元素
    QGraphicsScene *scene;
    QGraphicsPixmapItem *bgItem;
    QGraphicsPixmapItem *titleItem;
    QGraphicsRectItem *pauseMask;

    // 教程界面
    QGraphicsRectItem *tutorialMask;
    QGraphicsPixmapItem *tutorialImage;
    QGraphicsProxyWidget *closeTutorialWidget;

    // 暂停/继续按钮 + 新增暂停关闭按钮
    QGraphicsProxyWidget *pauseWidget;
    QGraphicsProxyWidget *resumeWidget;
    QGraphicsProxyWidget *pauseCloseWidget;

    // 游戏核心元素
    QGraphicsPixmapItem *playerItem;
    QGraphicsTextItem *scoreText;
    QGraphicsTextItem *gameOverText;
    QList<QGraphicsPixmapItem *> lifeHearts;
    QList<QGraphicsPixmapItem *> obstacles;
    QList<QGraphicsPixmapItem *> coins;
    QList<QGraphicsPixmapItem *> clouds;
    QGraphicsLineItem *laneLine1;
    QGraphicsLineItem *laneLine2;

    // 菜单按钮
    QGraphicsProxyWidget *startWidget;
    QGraphicsProxyWidget *restartWidget;
    QGraphicsProxyWidget *exitWidget;
    QGraphicsProxyWidget *tutorialWidget;

    // 音频系统
    QMediaPlayer *bgmPlayer;
    QAudioOutput *audioOutput;
    QMediaPlayer *jumpPlayer;
    QAudioOutput *jumpAudio;
    QMediaPlayer *slidePlayer;
    QAudioOutput *slideAudio;
    QMediaPlayer *swingPlayer;
    QAudioOutput *swingAudio;
    QMediaPlayer *hitPlayer;
    QAudioOutput *hitAudio;
    QMediaPlayer *coinPlayer;
    QAudioOutput *coinAudio;
    QMediaPlayer *eatPlayer;
    QAudioOutput *eatAudio;
    QMediaPlayer *bikePlayer;
    QAudioOutput *bikeAudio;

    // 定时器
    QTimer *timer;
    QTimer *spawnTimer;
    QTimer *slideTimer;

    // 游戏参数
    GameState currentState;
    PlayerAction currentAction;
    int currentLane;
    int score;
    int lives;
    qreal gameSpeed;
    bool hasBike;
    bool isInvincible;
    bool isMuted;
    bool wantsToSlide;

    // 角色动画
    qreal jumpVelocity;
    int frameCounter;
    int currentFrameIndex;
    QList<QPixmap> runFrames;
    QList<QPixmap> bikeFrames;
};

#endif // JUST_TEST_H