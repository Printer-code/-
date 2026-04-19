#ifndef JUST_TEST_H
#define JUST_TEST_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QList>
#include <QGraphicsProxyWidget>
#include <QDateTime>

// 游戏状态枚举
enum GameState
{
    TITLE,
    PLAYING,
    PAUSED,
    GAMEOVER
};
// 赛道枚举 (0:左, 1:中, 2:右)
enum Lane
{
    LEFT_LANE = 0,
    MID_LANE = 1,
    RIGHT_LANE = 2
};
// 玩家动作状态
enum PlayerAction
{
    RUNNING,
    JUMPING,
    SLIDING
};
// 障碍物类型
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

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void gameLoop();
    void spawnEntities();
    void resetPlayerAction();

    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onQuitClicked();
    void onToggleVolumeClicked();
    void onRestartClicked();

    void endInvincible();
    void clearEffect();

private:
    void initUI();
    void initAudio();
    void resetGame();
    void setGameState(GameState state);
    void updatePerspectiveScale(QGraphicsPixmapItem *item);

    QGraphicsScene *scene;
    QTimer *timer;
    QTimer *spawnTimer;
    QTimer *slideTimer;

    GameState currentState;
    int currentLane;
    PlayerAction currentAction;
    double gameSpeed;
    int score;
    int lives;
    bool hasBike;
    bool isMuted;
    bool isInvincible;
    bool wantsToSlide;

    QGraphicsPixmapItem *bgItem;
    QGraphicsPixmapItem *playerItem;
    QGraphicsPixmapItem *bikeIcon;
    QGraphicsTextItem *scoreText;
    QList<QGraphicsPixmapItem *> lifeHearts;

    QList<QGraphicsPixmapItem *> obstacles;
    QList<QGraphicsPixmapItem *> coins;
    QList<QGraphicsPixmapItem *> clouds;

    QGraphicsProxyWidget *startWidget;
    QGraphicsPixmapItem *btnPause;
    QGraphicsPixmapItem *btnResume;
    QGraphicsPixmapItem *btnQuit;
    QGraphicsPixmapItem *btnVolume;

    QMediaPlayer *bgmPlayer;
    QAudioOutput *audioOutput;

    // 音效播放器
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
    QMediaPlayer *bikePlayer; // 【新增】自行车音效播放器
    QAudioOutput *bikeAudio;

    QList<QPixmap> runFrames;
    QList<QPixmap> bikeFrames;
    int currentFrameIndex;
    int frameCounter;

    QGraphicsTextItem *gameOverText;
    QGraphicsProxyWidget *restartWidget;
    QGraphicsProxyWidget *exitWidget;
    QGraphicsRectItem *pauseMask;

    QGraphicsLineItem *laneLine1;
    QGraphicsLineItem *laneLine2;

    float jumpVelocity;
};

#endif // JUST_TEST_H