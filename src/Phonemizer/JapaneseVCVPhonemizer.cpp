#include "Phonemizer/JapaneseVCVPhonemizer.h"
#include "Phonemizer/AliasResolver.h"
#include "Singer/Singer.h"

namespace myvocal {

QString JapaneseVCVPhonemizer::name() const
{
    return QStringLiteral("Japanese VCV");
}

QString JapaneseVCVPhonemizer::vowelOf(const QString& lyric) const
{
    static const QMap<QString, QString> vowels = {
        {QStringLiteral("あ"), QStringLiteral("a")}, {QStringLiteral("い"), QStringLiteral("i")},
        {QStringLiteral("う"), QStringLiteral("u")}, {QStringLiteral("え"), QStringLiteral("e")},
        {QStringLiteral("お"), QStringLiteral("o")}, {QStringLiteral("か"), QStringLiteral("a")},
        {QStringLiteral("き"), QStringLiteral("i")}, {QStringLiteral("く"), QStringLiteral("u")},
        {QStringLiteral("け"), QStringLiteral("e")}, {QStringLiteral("こ"), QStringLiteral("o")},
        {QStringLiteral("さ"), QStringLiteral("a")}, {QStringLiteral("し"), QStringLiteral("i")},
        {QStringLiteral("す"), QStringLiteral("u")}, {QStringLiteral("せ"), QStringLiteral("e")},
        {QStringLiteral("そ"), QStringLiteral("o")}};
    return vowels.value(lyric, lyric);
}

std::vector<Phoneme> JapaneseVCVPhonemizer::process(const std::vector<Note>& notes,
                                                     const Singer& singer)
{
    AliasResolver resolver(singer);
    std::vector<Phoneme> result;
    result.reserve(notes.size());
    QString previousVowel = QStringLiteral("a");

    for (size_t i = 0; i < notes.size(); ++i) {
        const QString lyric = notes[i].getLyric().trimmed();
        const QString alias = (i == 0)
            ? lyric
            : previousVowel + QStringLiteral(" ") + lyric;

        // Deliberately do not fall back to the raw lyric.  The generated alias
        // must be the alias sent to the renderer, and Renderer reports a
        // Missing alias error when the singer does not contain it.
        const auto entry = resolver.resolve(alias);
        Phoneme p{alias,
                  static_cast<double>(notes[i].getStartTick()),
                  static_cast<double>(notes[i].getDurationTick()),
                  0, 0, 0, 0, 0, i};
        if (entry) {
            p.preutterance = entry->preutterance;
            p.overlap = entry->overlap;
            p.offset = entry->offset;
            p.consonant = entry->consonant;
            p.cutoff = entry->cutoff;
        }
        result.push_back(std::move(p));
        previousVowel = vowelOf(lyric);
    }
    return result;
}

}