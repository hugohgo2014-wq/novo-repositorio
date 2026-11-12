// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Jareer Abdullah and Concat contributors

//! The optional fake PRO+ usage-limit joke. This is deliberately cosmetic:
//! no edit, playback, import, or export path checks this state.

use std::time::{Duration, SystemTime, UNIX_EPOCH};

use super::Studio;
use crate::host::Shell;
use crate::ui::App;

const FAKE_LIMIT_SECONDS: u64 = 2 * 60 * 60;
const ROAST_DELAY_SECONDS: u64 = 3 * 60;

impl Studio {
    /// Start the joke's foreground-use timer and honour a roast that became
    /// due while the app was closed. Called once after the shell is installed.
    pub fn start_meme_timer(&mut self, app: &App) {
        if self.meme_limit_tick(false) {
            self.publish_status(app);
        }

        self.meme_timer.start(
            slint::TimerMode::Repeated,
            Duration::from_secs(1),
            || {
                Shell::with(|shell, app| {
                    let mut studio = shell.studio.borrow_mut();
                    if studio.meme_limit_tick(true) {
                        studio.publish_status(&app);
                    }
                });
            },
        );
    }

    /// Advance foreground use by one second, or only check a persisted due
    /// roast at startup. Returns true only when a new toast was raised.
    pub(crate) fn meme_limit_tick(&mut self, count_usage: bool) -> bool {
        if !self.prefs.meme_fake_limit_enabled() {
            return false;
        }

        if count_usage && self.prefs.meme_fake_limit_stage == 0 {
            self.prefs.meme_fake_limit_usage_seconds =
                self.prefs.meme_fake_limit_usage_seconds.saturating_add(1);
        }

        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();
        let notice = match self.prefs.meme_fake_limit_stage {
            0 if self.prefs.meme_fake_limit_usage_seconds >= FAKE_LIMIT_SECONDS => {
                self.prefs.meme_fake_limit_stage = 1;
                self.prefs.meme_fake_limit_roast_at = Some(now.saturating_add(ROAST_DELAY_SECONDS));
                Some(
                    "PAGUE PARA CONTINUAR! COMPRE PLANO PRO PLUS PRA USAR O EXPORTADOR 4K! Sua demo de 2 horas acabou. (É meme: nada foi bloqueado.)",
                )
            }
            1 if self
                .prefs
                .meme_fake_limit_roast_at
                .is_some_and(|deadline| now >= deadline) =>
            {
                self.prefs.meme_fake_limit_stage = 2;
                self.prefs.meme_fake_limit_roast_at = None;
                Some(
                    "SEU LISO DO CARALHO KKKKK! POBRE, PASSANDO FOME, MÃO DE VACA, PÉ-RACHADO, QUEBRADO E CALOTEIRO DE PIX! QUER EXPORTAR EM 4K COM O SALDO NEGATIVO? VAI EDITAR NO BLOCO DE NOTAS, MISERÁVEL! RELAXA: É SÓ MEME, NADA FOI BLOQUEADO.",
                )
            }
            _ => None,
        };

        let save_progress = count_usage
            && self.prefs.meme_fake_limit_stage == 0
            && self.prefs.meme_fake_limit_usage_seconds > 0
            && self.prefs.meme_fake_limit_usage_seconds.is_multiple_of(60);
        if notice.is_some() || save_progress {
            self.prefs.save(&self.host.dirs);
        }
        if let Some(message) = notice {
            self.notify(message, false);
            true
        } else {
            false
        }
    }
}
