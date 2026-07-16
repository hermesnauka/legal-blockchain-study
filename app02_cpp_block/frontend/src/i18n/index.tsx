import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import { en } from './en'
import type { Dict } from './en'
import { pl } from './pl'

export type Lang = 'en' | 'pl'

const STORAGE_KEY = 'ledger-lang'
const dicts: Record<Lang, Dict> = { en, pl }

interface I18nValue {
  lang: Lang
  t: Dict
  setLang: (l: Lang) => void
}

const I18nContext = createContext<I18nValue>({ lang: 'en', t: en, setLang: () => {} })

function initialLang(): Lang {
  // Priority: explicit ?lang= link > persisted choice > browser locale.
  const fromUrl = new URLSearchParams(window.location.search).get('lang')
  if (fromUrl === 'en' || fromUrl === 'pl') return fromUrl
  const stored = localStorage.getItem(STORAGE_KEY)
  if (stored === 'en' || stored === 'pl') return stored
  return navigator.language.toLowerCase().startsWith('pl') ? 'pl' : 'en'
}

export function I18nProvider({ children }: { children: ReactNode }) {
  const [lang, setLangState] = useState<Lang>(initialLang)
  const setLang = useCallback((l: Lang) => {
    localStorage.setItem(STORAGE_KEY, l)
    setLangState(l)
  }, [])
  // Keep <html lang> in sync from first render on, so screen readers and
  // hyphenation pick the right language even before any switcher click.
  useEffect(() => {
    document.documentElement.lang = lang
  }, [lang])
  const value = useMemo<I18nValue>(() => ({ lang, t: dicts[lang], setLang }), [lang, setLang])
  return <I18nContext.Provider value={value}>{children}</I18nContext.Provider>
}

export function useI18n(): I18nValue {
  return useContext(I18nContext)
}
