import { useState } from 'react'
import type { ReactNode } from 'react'

/** Monospace hash rendered truncated with full value on hover. */
export function Hash({ value, len = 16 }: { value: string; len?: number }) {
  if (!value) return <code className="hash">—</code>
  const short = value.length > len ? `${value.slice(0, len)}…` : value
  return (
    <code className="hash" title={value}>
      {short}
    </code>
  )
}

export function Card({ title, children, className }: { title?: ReactNode; children: ReactNode; className?: string }) {
  return (
    <section className={className ? `card ${className}` : 'card'}>
      {title !== undefined && <h3 className="card-title">{title}</h3>}
      {children}
    </section>
  )
}

export function Accordion({ header, children, defaultOpen = false }: { header: ReactNode; children: ReactNode; defaultOpen?: boolean }) {
  const [open, setOpen] = useState(defaultOpen)
  return (
    <div className={open ? 'accordion open' : 'accordion'}>
      <button className="accordion-header" onClick={() => setOpen(!open)} aria-expanded={open}>
        <span className="accordion-chevron">{open ? '▾' : '▸'}</span>
        {header}
      </button>
      {open && <div className="accordion-body">{children}</div>}
    </div>
  )
}

export function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="field">
      <span className="field-label">{label}</span>
      {children}
    </label>
  )
}

export function Toast({ message }: { message: string | null }) {
  if (!message) return null
  return <div className="toast">{message}</div>
}

export function formatTime(ts: number, lang: string): string {
  return new Date(ts).toLocaleString(lang === 'pl' ? 'pl-PL' : 'en-GB')
}
