import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { createBrowserRouter, RouterProvider } from 'react-router-dom'
import { I18nProvider } from './i18n'
import { Layout } from './components/Layout'
import { LedgerPage } from './pages/LedgerPage'
import { NetworkPage } from './pages/NetworkPage'
import { ConsensusPage } from './pages/ConsensusPage'
import { SecurityPage } from './pages/SecurityPage'
import { UseCasesPage } from './pages/UseCasesPage'
import { ContractsPage } from './pages/ContractsPage'
import { NftPage } from './pages/NftPage'
import { CrowdfundingPage } from './pages/CrowdfundingPage'
import './styles.css'

const router = createBrowserRouter([
  {
    path: '/',
    element: <Layout />,
    children: [
      { index: true, element: <LedgerPage /> },
      { path: 'network', element: <NetworkPage /> },
      { path: 'consensus', element: <ConsensusPage /> },
      { path: 'security', element: <SecurityPage /> },
      { path: 'usecases', element: <UseCasesPage /> },
      { path: 'contracts', element: <ContractsPage /> },
      { path: 'nft', element: <NftPage /> },
      { path: 'crowdfunding', element: <CrowdfundingPage /> },
    ],
  },
])

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <I18nProvider>
      <RouterProvider router={router} />
    </I18nProvider>
  </StrictMode>,
)
