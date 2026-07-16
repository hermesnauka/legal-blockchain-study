"""Sector smart contracts: medical consent (GDPR-safe) and agri forward-only traceability."""
from django.test import SimpleTestCase

from ledger.contracts import AgriSupplyChainContract, MedicalConsentContract


class ContractsTest(SimpleTestCase):

    def test_medical_consent_grant_and_revoke_are_both_recorded(self):
        medical = MedicalConsentContract()

        grant_result = medical.execute({
            "patientId": "patient-1", "granteeId": "dr-who",
            "scope": "IMAGING", "granted": "true"})
        self.assertTrue(grant_result.accepted)
        self.assertTrue(medical.has_consent("patient-1", "dr-who", "IMAGING"))
        self.assertEqual(set(grant_result.record.keys()),
                         {"contract", "patientId", "granteeId", "scope", "granted"})

        medical.execute({
            "patientId": "patient-1", "granteeId": "dr-who",
            "scope": "IMAGING", "granted": "false"})
        self.assertFalse(medical.has_consent("patient-1", "dr-who", "IMAGING"))
        self.assertEqual(len(medical.history_of("patient-1")), 2)

    def test_medical_consent_enforces_closed_scope_vocabulary_and_required_fields(self):
        medical = MedicalConsentContract()

        with self.assertRaises(ValueError):
            medical.execute({"patientId": "p", "granteeId": "dr-who",
                             "scope": "TELEPATHY", "granted": "true"})
        with self.assertRaises(ValueError):
            medical.execute({"patientId": "p"})

        self.assertEqual(medical.history_of("p"), [])

    def test_agri_trail_is_forward_only_from_harvest_to_retail(self):
        agri = AgriSupplyChainContract()

        def event(stage: str) -> dict:
            return {"batchId": "B-1", "stage": stage, "actor": "farmer",
                    "location": "field-1", "details": ""}

        with self.assertRaises(ValueError):
            agri.execute(event("TRANSPORT"))

        agri.execute(event("HARVEST"))
        agri.execute(event("PROCESSING"))
        agri.execute(event("TRANSPORT"))

        with self.assertRaises(ValueError):
            agri.execute(event("PROCESSING"))

        agri.execute(event("TRANSPORT"))  # same-stage repeat is allowed

        self.assertEqual([e["stage"] for e in agri.trail_of("B-1")],
                         ["HARVEST", "PROCESSING", "TRANSPORT", "TRANSPORT"])

        with self.assertRaises(ValueError):
            agri.execute(event("SPACE_ELEVATOR"))
