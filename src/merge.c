#include <string.h>
#include "utils.h"

/* Attempt fast forward merge (simply add commits from other branch)
 * Example: https://github.com/libgit2/libgit2/blob/master/examples/merge.c#L116-L182
 */
SEXP R_git_merge_fast_forward(SEXP ptr, SEXP ref){
  git_reference *head;
  git_reference *target;
  git_object *revision = NULL;
  git_annotated_commit *commit;
  git_repository *repo = get_git_repository(ptr);
  git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
  opts.checkout_strategy = GIT_CHECKOUT_SAFE;

  /* Lookup current and target tree state */
  bail_if(git_repository_head(&head, repo), "git_repository_head");
  bail_if(git_revparse_single(&revision, repo, CHAR(STRING_ELT(ref, 0))), "git_revparse_single");
  bail_if(git_annotated_commit_lookup(&commit, repo, git_object_id(revision)), "git_annotated_commit_lookup");

  /* Test if they can safely be merged */
  git_merge_analysis_t analysis;
  git_merge_preference_t preference;
  const git_annotated_commit *ccommit = commit;
  bail_if(git_merge_analysis(&analysis, &preference, repo, &ccommit, 1), "git_merge_analysis");
  git_annotated_commit_free(commit);

  /* Check analysis output */
  if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE){
    REprintf("Already up-to-date\n");
    goto done;
  } else if(analysis & GIT_MERGE_ANALYSIS_FASTFORWARD || analysis & GIT_MERGE_ANALYSIS_UNBORN){
    REprintf("Performing fast forward\n");
    bail_if(git_checkout_tree(repo, revision, &opts), "git_checkout_tree");
    bail_if(git_reference_set_target(&target, head, git_object_id(revision), NULL), "git_reference_set_target");
    git_reference_free(target);
  } else {
    REprintf("Fast forward not possible\n");
  }

done:
  git_reference_free(head);
  git_object_free(revision);
  return ptr;
}

SEXP R_git_merge_base(SEXP ptr, SEXP ref1, SEXP ref2){
  git_object *t1 = NULL;
  git_object *t2 = NULL;
  git_oid base = {{0}};
  git_repository *repo = get_git_repository(ptr);
  bail_if(git_revparse_single(&t1, repo, CHAR(STRING_ELT(ref1, 0))), "git_revparse_single");
  bail_if(git_revparse_single(&t2, repo, CHAR(STRING_ELT(ref2, 0))), "git_revparse_single");
  bail_if(git_merge_base(&base, repo, git_object_id(t1), git_object_id(t2)), "git_merge_base");
  git_object_free(t1);
  git_object_free(t2);
  return Rf_mkString(git_oid_tostr_s(&base));
}

static const char *analysis_to_str(git_merge_analysis_t x){
  static const char *none = "none";
  static const char *normal = "normal";
  static const char *uptodate = "up_to_date";
  static const char *fastforward = "fastforward";
  static const char *unborn = "unborn";
  switch(x){
  case GIT_MERGE_ANALYSIS_NONE:
    return none;
  case GIT_MERGE_ANALYSIS_NORMAL:
    return normal;
  case GIT_MERGE_ANALYSIS_UP_TO_DATE:
    return uptodate;
  case GIT_MERGE_ANALYSIS_FASTFORWARD:
    return fastforward;
  case GIT_MERGE_ANALYSIS_UNBORN:
    return unborn;
  }
  return none;
}

SEXP R_git_merge_analysis(SEXP ptr, SEXP refs, SEXP target){
  git_reference *t = NULL;
  git_repository *repo = get_git_repository(ptr);
  bail_if(git_reference_lookup(&t, repo, CHAR(STRING_ELT(target, 0))), "git_reference_lookup");
  int n = Rf_length(refs);
  git_annotated_commit *commits[n];
  for(int i = 0; i < n; i++){
    bail_if(git_annotated_commit_from_revspec(&commits[i], repo, CHAR(STRING_ELT(refs, i))),
            "git_annotated_commit_from_revspec");
  }
  git_merge_analysis_t analysis_out;
  git_merge_preference_t preference_out;
  int res = git_merge_analysis_for_ref(&analysis_out, &preference_out, repo, t, (const git_annotated_commit**) commits, n);
  for(int i = 0; i < n; i++)
    git_annotated_commit_free(commits[i]);
  bail_if(res, "git_merge_analysis");
  return Rf_mkString(analysis_to_str(analysis_out));
}
