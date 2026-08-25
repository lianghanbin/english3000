package org.liang.english3000;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import java.util.Locale;

/**
 * 极简 Android 原生 TTS 封装。
 * C++ 通过 QJniObject 调用 speak(text),走系统本地引擎,
 * 离线、毫秒级发音,解决 edge-tts 联网"慢半拍"的问题。
 *
 * 设计:
 *  - 单例,进程内复用同一个 TextToSpeech 实例。
 *  - 初始化是异步的(onInit 回调);在就绪前调用 speak 会排队,
 *    就绪后自动播放最后一次请求,保证第一声也能响。
 *  - 新的 speak 会打断正在读的内容,符合背单词点一个读一个的场景。
 */
public class TtsHelper implements TextToSpeech.OnInitListener {
    private static TtsHelper sInstance;
    private TextToSpeech mTts;
    private boolean mReady = false;
    private String mPendingText = null;
    private float mRate = 0.95f;

    public static TtsHelper get(Context context) {
        if (sInstance == null) {
            sInstance = new TtsHelper(context.getApplicationContext());
        }
        return sInstance;
    }

    private TtsHelper(Context context) {
        try {
            mTts = new TextToSpeech(context.getApplicationContext(), this);
        } catch (Throwable t) {
            mTts = null;
        }
    }

    @Override
    public void onInit(int status) {
        if (mTts == null) return;
        if (status == TextToSpeech.SUCCESS) {
            // 优先美式英语,找不到就用系统默认。
            int r = mTts.setLanguage(Locale.US);
            if (r == TextToSpeech.LANG_MISSING_DATA
                    || r == TextToSpeech.LANG_NOT_SUPPORTED) {
                mTts.setLanguage(Locale.getDefault());
            }
            mTts.setSpeechRate(mRate);
            mReady = true;
            if (mPendingText != null) {
                String t = mPendingText;
                mPendingText = null;
                speak(t);
            }
        }
    }

    public void speak(String text) {
        if (text == null || text.isEmpty()) return;
        if (mTts == null) return;
        if (!mReady) {
            mPendingText = text;
            return;
        }
        mTts.stop();
        mTts.speak(text, TextToSpeech.QUEUE_FLUSH, null, "en3k_" + System.nanoTime());
    }

    public void stop() {
        if (mTts != null) mTts.stop();
    }

    /** 返回当前系统默认 TTS 引擎的包名(如 com.xiaomi.mibrain.speech)。
     *  未初始化或无引擎时返回空串。 */
    public String defaultEngine() {
        if (mTts == null) return "";
        try {
            String e = mTts.getDefaultEngine();
            return e != null ? e : "";
        } catch (Throwable t) {
            return "";
        }
    }

    public void shutdown() {
        if (mTts != null) {
            mTts.stop();
            mTts.shutdown();
            mTts = null;
        }
        mReady = false;
    }
}
