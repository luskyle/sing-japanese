use lindera::{
    dictionary::{load_dictionary_from_kind, DictionaryKind},
    mode::Mode,
    segmenter::Segmenter,
    tokenizer::Tokenizer,
};
use serde::Serialize;
use std::sync::OnceLock;

static TOKENIZER: OnceLock<Result<Tokenizer, String>> = OnceLock::new();

#[derive(Debug, Serialize)]
pub struct Segment {
    text: String,
    reading: Option<String>,
    unknown: bool,
}

fn has_kanji(text: &str) -> bool {
    text.chars().any(|c| {
        matches!(c as u32,
            0x3400..=0x4dbf | 0x4e00..=0x9fff | 0xf900..=0xfaff | 0x20000..=0x323af
        ) || c == '々'
            || c == '〆'
    })
}

fn hiragana(text: &str) -> String {
    text.chars()
        .map(|c| {
            if ('ァ'..='ヶ').contains(&c) {
                char::from_u32(c as u32 - 0x60).unwrap_or(c)
            } else {
                c
            }
        })
        .collect()
}

pub fn annotate(text: &str) -> Result<Vec<Segment>, String> {
    if text.chars().count() > 20_000 {
        return Err("一次最多注音 20,000 个字符，请分段处理。".into());
    }
    let tokenizer = TOKENIZER
        .get_or_init(|| {
            let dictionary = load_dictionary_from_kind(DictionaryKind::IPADIC)
                .map_err(|error| format!("无法加载本地日语词典：{error}"))?;
            Ok(Tokenizer::new(Segmenter::new(
                Mode::Normal,
                dictionary,
                None,
            )))
        })
        .as_ref()
        .map_err(Clone::clone)?;
    let mut tokens = tokenizer
        .tokenize(text)
        .map_err(|error| format!("日语注音分析失败：{error}"))?;
    let mut result = Vec::new();
    let mut end = 0;
    for token in &mut tokens {
        if token.byte_start < end
            || token.byte_end < token.byte_start
            || !text.is_char_boundary(token.byte_start)
            || !text.is_char_boundary(token.byte_end)
            || token.byte_end > text.len()
        {
            return Err("词典返回了无效的文字位置，已停止注音。".into());
        }
        if token.byte_start > end {
            result.push(Segment {
                text: text[end..token.byte_start].into(),
                reading: None,
                unknown: false,
            });
        }
        let surface = &text[token.byte_start..token.byte_end];
        let reading = if has_kanji(surface) {
            token
                .details()
                .get(7)
                .filter(|reading| !reading.is_empty() && **reading != "*")
                .map(|reading| hiragana(reading))
        } else {
            None
        };
        result.push(Segment {
            text: surface.into(),
            unknown: has_kanji(surface) && reading.is_none(),
            reading,
        });
        end = token.byte_end;
    }
    if end < text.len() {
        result.push(Segment {
            text: text[end..].into(),
            reading: None,
            unknown: has_kanji(&text[end..]),
        });
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn annotates_words_and_preserves_every_original_character() {
        let text = "今日、学校へ行く。\r\n\n  カタカナとひらがな <b>空</b> 😀";
        let segments = annotate(text).unwrap();
        assert_eq!(
            segments.iter().map(|s| s.text.as_str()).collect::<String>(),
            text
        );
        assert!(segments
            .iter()
            .any(|s| s.text == "学校" && s.reading.as_deref() == Some("がっこう")));
        assert!(segments
            .iter()
            .any(|s| s.text == "今日" && s.reading.as_deref() == Some("きょう")));
        assert!(segments
            .iter()
            .filter(|s| !has_kanji(&s.text))
            .all(|s| s.reading.is_none()));
    }

    #[test]
    fn handles_empty_unknown_and_over_limit_input() {
        assert!(annotate("").unwrap().is_empty());
        assert!(annotate("𠮷").unwrap().iter().any(|s| s.unknown));
        assert!(annotate(&"あ".repeat(20_001)).is_err());
        assert_eq!(hiragana("ガッコウ"), "がっこう");
    }
}
