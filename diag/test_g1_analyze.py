import unittest
from g1_analyze import analyze


def record(phase=2,skip=0,demand=1000,page=0):
    return ('I g1_ui: phase=%d page=%d locale=1 home=0 hold=0 kind=0 fresh=1 skip=%d '
            'held=0 release=0 allowed=1 n=1 pre=10000 post=10000 low=%d '
            'block_pre=8000 block_post=8000 block_low=4000 demand=%d '
            'max_text_bytes=100 max_text_seen=30')%(phase,page,skip,10000-demand,demand)


class G1AnalyzeTest(unittest.TestCase):
    def test_skip_never_becomes_zero_demand_or_qualification(self):
        r=analyze(record(skip=1,demand=0))
        self.assertIsNone(r['observed_max_checkpoint_demand_bytes'])
        self.assertIsNone(r['arithmetic_candidate_bytes_NOT_QUALIFIED'])
        self.assertEqual('NOT_QUALIFIED',r['threshold_status'])
        self.assertEqual('NOT_PROVEN',r['touch_contact_evidence'])
        self.assertEqual([(0,1)],r['skipped_page_locale_without_measured_render'])

    def test_formula_remains_unqualified_and_coverage_separate(self):
        r=analyze(record(demand=8000)+'\n'+record(skip=1,page=2))
        self.assertEqual(16000,r['arithmetic_candidate_bytes_NOT_QUALIFIED'])
        self.assertIn((2,1),r['skipped_page_locale_without_measured_render'])
        self.assertEqual('NOT_QUALIFIED',r['threshold_status'])

    def test_incomplete_mismatched_or_overflow_evidence_cannot_pass(self):
        r=analyze('I g1_ui: phase=1\n'+record().replace('demand=1000','demand=1')+'\ng1_gate: dropped=1')
        self.assertEqual(2,r['invalid_ui_lines'])
        self.assertTrue(r['probe_overflow'])
        self.assertIsNone(r['observed_max_checkpoint_demand_bytes'])
