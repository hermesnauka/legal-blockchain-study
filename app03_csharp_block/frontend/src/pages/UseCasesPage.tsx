import { useI18n } from '../i18n'

/**
 * Business use-case dashboards: seven sector cards (DRM, logistics,
 * medicine, agriculture, higher education, democratic voting, communities)
 * each tying a compliance problem to a live feature of this application.
 */
export function UseCasesPage() {
  const { t } = useI18n()

  return (
    <div>
      <h1>{t.usecases.title}</h1>
      <p className="page-intro">{t.usecases.intro}</p>

      <div className="usecase-grid">
        {t.usecases.cards.map((card) => (
          <div className="usecase-card" key={card.title}>
            <h3>{card.title}</h3>
            <div className="label">{t.usecases.problem}</div>
            <p>{card.problem}</p>
            <div className="label">{t.usecases.solution}</div>
            <p>{card.solution}</p>
            <div className="label">{t.usecases.compliance}</div>
            <p>{card.compliance}</p>
            <div className="label">{t.usecases.liveFeature}</div>
            <p className="live">{card.liveFeature}</p>
          </div>
        ))}
      </div>
    </div>
  )
}
