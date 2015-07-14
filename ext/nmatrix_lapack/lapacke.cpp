#include "lapacke/utils/lapacke_dtr_nancheck.c"
#include "lapacke/utils/lapacke_dpo_nancheck.c"
#include "lapacke/utils/lapacke_ztr_nancheck.c"
#include "lapacke/utils/lapacke_str_trans.c"
#include "lapacke/utils/lapacke_cge_nancheck.c"
#include "lapacke/utils/lapacke_dtr_trans.c"
#include "lapacke/utils/lapacke_zge_nancheck.c"
#include "lapacke/utils/lapacke_cpo_trans.c"
#include "lapacke/utils/lapacke_zpo_nancheck.c"
#include "lapacke/utils/lapacke_dge_nancheck.c"
#include "lapacke/utils/lapacke_ctr_trans.c"
#include "lapacke/utils/lapacke_ztr_trans.c"
#include "lapacke/utils/lapacke_cge_trans.c"
#include "lapacke/utils/lapacke_dpo_trans.c"
#include "lapacke/utils/lapacke_spo_trans.c"
#include "lapacke/utils/lapacke_ctr_nancheck.c"
#include "lapacke/utils/lapacke_cpo_nancheck.c"
#include "lapacke/utils/lapacke_str_nancheck.c"
#include "lapacke/utils/lapacke_xerbla.c"
#include "lapacke/utils/lapacke_dge_trans.c"
#include "lapacke/utils/lapacke_sge_nancheck.c"
#include "lapacke/utils/lapacke_zge_trans.c"
#include "lapacke/utils/lapacke_sge_trans.c"
#include "lapacke/utils/lapacke_lsame.c"
#include "lapacke/utils/lapacke_spo_nancheck.c"
#include "lapacke/utils/lapacke_zpo_trans.c"
#include "lapacke/src/lapacke_zgesvd_work.c"
#include "lapacke/src/lapacke_cgetri_work.c"
#include "lapacke/src/lapacke_zgesdd_work.c"
#include "lapacke/src/lapacke_zpotrf_work.c"
#include "lapacke/src/lapacke_dgetri_work.c"
#include "lapacke/src/lapacke_dpotri_work.c"
#include "lapacke/src/lapacke_sgesvd_work.c"
#include "lapacke/src/lapacke_cpotri_work.c"
#include "lapacke/src/lapacke_dgetrf_work.c"
#include "lapacke/src/lapacke_cpotrf_work.c"
#include "lapacke/src/lapacke_sgetrs_work.c"
#include "lapacke/src/lapacke_zgetri_work.c"
#include "lapacke/src/lapacke_zgetri.c"
#include "lapacke/src/lapacke_spotrs_work.c"
#include "lapacke/src/lapacke_zpotrs.c"
#include "lapacke/src/lapacke_cpotrs_work.c"
#include "lapacke/src/lapacke_dgesdd_work.c"
#include "lapacke/src/lapacke_sgetrf_work.c"
#include "lapacke/src/lapacke_cgeev.c"
#include "lapacke/src/lapacke_sgeev_work.c"
#include "lapacke/src/lapacke_spotri_work.c"
#include "lapacke/src/lapacke_dgesvd_work.c"
#include "lapacke/src/lapacke_dgetrs.c"
#include "lapacke/src/lapacke_dgesdd.c"
#include "lapacke/src/lapacke_cpotrf.c"
#include "lapacke/src/lapacke_dgeev.c"
#include "lapacke/src/lapacke_sgetrf.c"
#include "lapacke/src/lapacke_cgeev_work.c"
#include "lapacke/src/lapacke_dpotrs_work.c"
#include "lapacke/src/lapacke_dpotrs.c"
#include "lapacke/src/lapacke_zpotri_work.c"
#include "lapacke/src/lapacke_dgetri.c"
#include "lapacke/src/lapacke_sgesdd_work.c"
#include "lapacke/src/lapacke_dgesvd.c"
#include "lapacke/src/lapacke_sgetri_work.c"
#include "lapacke/src/lapacke_zgetrs.c"
#include "lapacke/src/lapacke_dpotrf_work.c"
#include "lapacke/src/lapacke_cgesvd_work.c"
#include "lapacke/src/lapacke_cpotrs.c"
#include "lapacke/src/lapacke_sgesdd.c"
#include "lapacke/src/lapacke_spotrs.c"
#include "lapacke/src/lapacke_cgetrs.c"
#include "lapacke/src/lapacke_zpotri.c"
#include "lapacke/src/lapacke_zgetrf.c"
#include "lapacke/src/lapacke_zgesdd.c"
#include "lapacke/src/lapacke_spotrf_work.c"
#include "lapacke/src/lapacke_cgesvd.c"
#include "lapacke/src/lapacke_dpotrf.c"
#include "lapacke/src/lapacke_cgesdd.c"
#include "lapacke/src/lapacke_dgeev_work.c"
#include "lapacke/src/lapacke_spotrf.c"
#include "lapacke/src/lapacke_sgeev.c"
#include "lapacke/src/lapacke_dgetrf.c"
#include "lapacke/src/lapacke_cgesdd_work.c"
#include "lapacke/src/lapacke_zpotrf.c"
#include "lapacke/src/lapacke_zgeev.c"
#include "lapacke/src/lapacke_spotri.c"
#include "lapacke/src/lapacke_zgeev_work.c"
#include "lapacke/src/lapacke_sgesvd.c"
#include "lapacke/src/lapacke_cpotri.c"
#include "lapacke/src/lapacke_zgetrf_work.c"
#include "lapacke/src/lapacke_cgetri.c"
#include "lapacke/src/lapacke_dpotri.c"
#include "lapacke/src/lapacke_cgetrf_work.c"
#include "lapacke/src/lapacke_zgetrs_work.c"
#include "lapacke/src/lapacke_sgetri.c"
#include "lapacke/src/lapacke_zgesvd.c"
#include "lapacke/src/lapacke_sgetrs.c"
#include "lapacke/src/lapacke_dgetrs_work.c"
#include "lapacke/src/lapacke_cgetrs_work.c"
#include "lapacke/src/lapacke_zpotrs_work.c"
#include "lapacke/src/lapacke_cgetrf.c"
