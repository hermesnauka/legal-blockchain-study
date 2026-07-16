import { useEffect, useState } from 'react'
import { NavLink, Outlet } from 'react-router-dom'
import { useI18n } from '../i18n'
import { API_STATUS_EVENT } from '../api/client'

export function Layout() {
  const { t, lang, setLang } = useI18n()
  const [online, setOnline] = useState(true)

  useEffect(() => {
    const onStatus = (e: Event) => setOnline((e as CustomEvent<boolean>).detail)
    window.addEventListener(API_STATUS_EVENT, onStatus)
    return () => window.removeEventListener(API_STATUS_EVENT, onStatus)
  }, [])

  const links: Array<[string, string]> = [
    ['/', t.nav.ledger],
    ['/network', t.nav.network],
    ['/consensus', t.nav.consensus],
    ['/security', t.nav.security],
    ['/usecases', t.nav.usecases],
    ['/contracts', t.nav.contracts],
    ['/nft', t.nav.nft],
    ['/crowdfunding', t.nav.crowdfunding],
  ]

  return (
    <div className="shell">
      <header className="topbar">
        <div className="brand">
          <span className="brand-mark">⛓</span>
          <div>
            <div className="brand-title">{t.app.title}</div>
            <div className="brand-sub">{t.app.subtitle}</div>
          </div>
        </div>
        <div className="lang-switcher" role="group" aria-label="Language">
          <button
            className={lang === 'en' ? 'lang active' : 'lang'}
            onClick={() => setLang('en')}
            title="English"
          >
            🇬🇧 EN
          </button>
          <button
            className={lang === 'pl' ? 'lang active' : 'lang'}
            onClick={() => setLang('pl')}
            title="Polski"
          >
            🇵🇱 PL
          </button>
        </div>
      </header>
      {!online && (
        <div className="offline-banner" role="alert">
          ⚠ {t.app.backendOffline}
        </div>
      )}
      <div className="body">
        <nav className="sidenav">
          {links.map(([to, label]) => (
            <NavLink key={to} to={to} end={to === '/'} className={({ isActive }) => (isActive ? 'navlink active' : 'navlink')}>
              {label}
            </NavLink>
          ))}
        </nav>
        <main className="content">
          <Outlet />
        </main>
      </div>
    </div>
  )
}
