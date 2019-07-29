/*
    Kalign - a multiple sequence alignment program

    Copyright 2006, 2019 Timo Lassmann

    This file is part of kalign.

    Kalign is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <xmmintrin.h>

#include "msa.h"
#include "sequence_distance.h"
#include "bisectingKmeans.h"

#include "euclidean_dist.h"

#include "alignment.h"
#include "pick_anchor.h"

struct node{
        struct node* left;
        struct node* right;
        int id;
};


struct kmeans_result{
        int* sl;
        int* sr;
        int nl;
        int nr;
        float score;
};

static struct kmeans_result* alloc_kmeans_result(int num_samples);
static void free_kmeans_results(struct kmeans_result* k);

struct node* upgma(float **dm,int* samples, int numseq);
struct node* alloc_node(void);

int label_internal(struct node*n, int label);
int* readbitree(struct node* p,int* tree);
void printTree(struct node* curr,int depth);
struct node* bisecting_kmeans(struct msa* msa, struct node* n, float** dm,int* samples,int numseq, int num_anchors,int num_samples,struct rng_state* rng);

float** pair_wu_fast_dist(struct msa* msa, struct aln_param* ap, int* num_anchors);

int unit_zero(float** d, int len_a, int len_b);


int sort_int_desc(const void *a, const void *b);

int random_tree(struct aln_param* ap, int numseq)
{
        int* tree = NULL;
        int* selection  = NULL;
        int num_profiles = 0;
        int i,j,c,g,f;

        tree = ap->tree;
        MMALLOC(selection, sizeof(int) * numseq);

        for(i =0; i < numseq;i++){
                selection[i] = i;
        }

        shuffle_arr_r(selection, numseq, ap->rng);

        c = numseq;
        g = numseq;

        f = 0;

        num_profiles = (numseq << 1) - 1;
        while(c != num_profiles){
                i = selection[0];
                j = selection[1];
                //fprintf(stdout,"Align %d %d -> %d\n",i,j,c);
                selection[0] = c;
                selection[1] = -1;


                tree[f] = i;
                f++;
                tree[f] = j;
                f++;
                tree[f] = c;
                f++;


                qsort(selection, g, sizeof(int) , sort_int_desc);
                /*for(i = 0; i < numseq;i++){
                        fprintf(stdout,"%d ",selection[i]);
                }
                fprintf(stdout,"\n");*/

                g--;
                shuffle_arr_r(selection, g, ap->rng);

                c++;

        }

        MFREE(selection);
        return OK;
ERROR:
        if(selection){
                MFREE(selection);
        }
        return FAIL;
}

int sort_int_desc(const void *a, const void *b)
{
           return ( *(int*)b - *(int*)a );
}


int build_tree_kmeans(struct msa* msa, struct aln_param* ap)
{
        //struct drand48_data randBuffer;
        struct node* root = NULL;
        float** dm = NULL;
        int* tree = NULL;
        int* samples = NULL;
        int* anchors = NULL;
        int num_anchors;
        int numseq;

        int i;

        ASSERT(msa != NULL, "No alignment.");
        //ASSERT(param != NULL, "No input parameters.");
        ASSERT(ap != NULL, "No alignment parameters.");


        tree = ap->tree;


        numseq = msa->numseq;

        DECLARE_TIMER(timer);
        /* pick anchors . */
        LOG_MSG("Calculating pairwise distances");
        START_TIMER(timer);
        RUNP(anchors = pick_anchor(msa, &num_anchors));

        RUNP(dm = d_estimation(msa, anchors, num_anchors,0));//les,int pair)
#ifdef HAVE_AVX2

        LOG_MSG("Running AVX code");
        //RUNP(dm = kmer_distance(aln,  anchors, num_anchors,10));

        //RUNP(dm = bpm_distance_thin(msa, anchors, num_anchors));
#else
        LOG_MSG("Running non AVX code");
        //if(msa->L > 5){
        //RUNP(dm = protein_wu_distance(msa, 58.8, anchors, num_anchors));
        //}else{
        //RUNP(dm = dna_wu_distance(msa, 61.08, anchors, num_anchors));
        //}
#endif
//RUNP(dm = bpm_distance(aln,anchors,num_anchors));
        /* normalize  */
        STOP_TIMER(timer);

        LOG_MSG("Done in %f sec.", GET_TIMING(timer));
        //unit_zero(dm, msa->numseq , num_anchors);
        MFREE(anchors);


        //dm = pair_wu_fast_dist(aln, ap, &num_anchors);




        /*int j;
        double r;
        float sum = 0.0;
        for(i = 0; i < aln->numseq;i++){
                sum = 0.0;
                for(j = 0; j < num_anchors;j++){
                        //drand48_r(&randBuffer, &r);
                        sum += dm[i][j];
                        //fprintf(stdout,"%f ",dm[i][j]);

                }
                if(sum == 0.0){
                        sum = 1.0;
                }
                for(j = 0; j < num_anchors;j++){
                        //drand48_r(&randBuffer, &r);
                        dm[i][j] = dm[i][j] / sum;
                        fprintf(stdout,"%f ",dm[i][j]);

                }
                fprintf(stdout,"\n");
                }

        */
        //RUNP(dm = pair_aln_dist(aln, ap, &num_anchors));

        /*for(i = 0; i < aln->numseq;i++){
                fprintf(stdout,"%d",i);
                for(j = 0; j <  num_anchors;j++){
                        fprintf(stdout," %f", dm[i][j]);
                }
                fprintf(stdout,"\n");
                }*/

        MMALLOC(samples, sizeof(int)* numseq);
        for(i = 0; i < numseq;i++){
                samples[i] = i;
        }



        //RUNP(root = alloc_node());

        START_TIMER(timer);

        LOG_MSG("Building guide tree.");


        RUNP(root = bisecting_kmeans(msa,root, dm, samples, numseq, num_anchors, numseq, ap->rng));
        STOP_TIMER(timer);

        LOG_MSG("Done in %f sec.", GET_TIMING(timer));
//        MFREE(samples);
        label_internal(root, numseq);
        //      printTree(root, 0);

        ap->tree[0] = 1;
        ap->tree = readbitree(root, ap->tree);
        for (i = 0; i < (numseq*3);i++){
                tree[i] = tree[i+1];
//                fprintf(stdout,"%d %d\n",tree[i], aln->num_profiles);
                //if(tree[i] = aln->num_profiles-1){
                //break;
                //}
        }

        MFREE(root);
        for(i =0 ; i < msa->numseq;i++){
                _mm_free(dm[i]);
        }
        MFREE(dm);

        return OK;
ERROR:
        return FAIL;
}

int unit_zero(float** d, int len_a, int len_b)
{

        float sum;
        float sq_sum;
        float mean;
        float variance;

        int i,j;

        for(j = 0; j < len_b;j++){
                sum = 0.0f;
                sq_sum = 0.0f;
                for(i = 0; i < len_a;i++){
                        sum += d[i][j];
                        sq_sum += d[i][j] * d[i][j];
                }
                mean = sum / (float) len_a;
                variance = sq_sum / (float) len_a - mean * mean;
                //fprintf(stdout,"%d %f %f \n",j,mean,variance);
                if(variance){
                        for(i = 0; i < len_a;i++){
                                d[i][j] = (d[i][j] - mean) / variance;
                        }
                }
                /*sum = 0.0f;
                sq_sum = 0.0f;
                for(i = 0; i < len_a;i++){
                        sum += d[i][j];
                        sq_sum += d[i][j] * d[i][j];
                }
                mean = sum / (float) len_a;
                variance = sq_sum / (float) len_a - mean * mean;
                fprintf(stdout,"%d %f %f \n",j,mean,variance);*/
        }
        return OK;
}

float** pair_wu_fast_dist(struct msa* msa, struct aln_param* ap, int* num_anchors)
{
        float** dm = NULL;
        int* anchors = NULL;
        ASSERT(msa != NULL,"No alignment");




        RUNP(anchors = pick_anchor(msa, num_anchors));

        //dm = protein_wu_distance(aln, 59.0,0, anchors, *num_anchors);
        //dm = bpm_distance(aln,anchors,*num_anchors);
        dm = kmer_distance(msa,  anchors, *num_anchors,10);
        /* normalize  */
        MFREE(anchors);
        return dm;
ERROR:
        return NULL;
}


struct node* bisecting_kmeans(struct msa* msa, struct node* n, float** dm,int* samples,int numseq, int num_anchors,int num_samples,struct rng_state* rng)
{


        struct kmeans_result* res_tmp = NULL;
        struct kmeans_result* best = NULL;
        struct kmeans_result* res_ptr = NULL;

        int tries = 50;
        int t_iter;
        int r;
        int* sl = NULL;
        int* sr = NULL;
        int num_l,num_r;
        float* w = NULL;
        float* wl = NULL;
        float* wr = NULL;
        float* cl = NULL;
        float* cr = NULL;
        float dl = 0.0f;
        float dr = 0.0f;
        float score;
        int i,j,s;
        int num_var;
        /* Pick random point (in samples !! ) */
        int stop = 0;

        if(num_samples < 100){
                float** dm = NULL;
                //dm = protein_wu_distance(aln, 58, 0, samples, num_samples);
                RUNP(dm = d_estimation(msa, samples, num_samples,1));// anchors, num_anchors,1));//les,int pair)
#ifdef HAVE_AVX2
                dm = bpm_distance_pair(msa, samples, num_samples);
#else
                LOG_MSG("Running non AVX code");
                if(msa->L > 5){
                        RUNP(dm = protein_wu_distance(msa, 58.8, samples, num_samples));
                }else{
                        RUNP(dm = dna_wu_distance(msa, 58.8, samples, num_samples));
                }
#endif
                //unit_zero(dm,  num_samples ,num_samples);
                //MFREE(n);
                //RUN(unit_zero(dm, num_samples, num_samples));
                n = upgma(dm,samples, num_samples);

                gfree(dm);
                MFREE(samples);
                return n;
        }
        /*if(num_samples == 1){
          n->id = samples[0];
          MFREE(samples);
          return n;
          }
          if(num_samples == 2){
          n = alloc_node();
          n->left = alloc_node();
          n->right = alloc_node();
          n->left->id = samples[0];
          n->right->id = samples[1];

          MFREE(samples);
          return n;

          }*/
        num_var = num_anchors / 8;
        if( num_anchors%8){
                num_var++;
        }
        num_var = num_var << 3;


        wr = _mm_malloc(sizeof(float) * num_var,32);
        wl = _mm_malloc(sizeof(float) * num_var,32);
        cr = _mm_malloc(sizeof(float) * num_var,32);
        cl = _mm_malloc(sizeof(float) * num_var,32);


        RUNP(best = alloc_kmeans_result(num_samples));
        RUNP(res_tmp = alloc_kmeans_result(num_samples));

        best->score = FLT_MAX;


        //MMALLOC(sl, sizeof(int) * num_samples);
        //MMALLOC(sr, sizeof(int) * num_samples);

        for(t_iter = 0;t_iter < tries;t_iter++){
                res_tmp->score = FLT_MAX;
                sl = res_tmp->sl;
                sr = res_tmp->sr;

                w = _mm_malloc(sizeof(float) * num_var,32);
                for(i = 0; i < num_var;i++){
                        w[i] = 0.0f;
                        wr[i] = 0.0f;
                        wl[i] = 0.0f;
                        cr[i] = 0.0f;
                        cl[i] = 0.0f;
                }
                for(i = 0; i < num_samples;i++){
                        s = samples[i];
                        for(j = 0; j < num_anchors;j++){
                                w[j] += dm[s][j];
                        }
                }

                for(j = 0; j < num_anchors;j++){
                        w[j] /= num_samples;
                }
                r = tl_random_int(rng  , num_samples);


                s = samples[r];
                //LOG_MSG("Selected %d\n",s);
                for(j = 0; j < num_anchors;j++){
                        cl[j] = dm[s][j];
                }

                for(j = 0; j < num_anchors;j++){
                        cr[j] = w[j] - (cl[j] - w[j]);
                        //      fprintf(stdout,"%f %f  %f\n", cl[j],cr[j],w[j]);
                }

                _mm_free(w);

                /* check if cr == cl - we have identical sequences  */
                s = 0;
                //LOG_MSG("COMP");
                for(j = 0; j < num_anchors;j++){
                        //fprintf(stdout,"%d\t%f %f  %f %d %e\n",j, cl[j],cr[j],w[j],s,cl[j] - cr[j]);
                        //if(cl[j] != cr[j]){
                        //fprintf(stdout," diff: %e  cutoff %e\n",fabsf(cl[j]-cr[j]),1.0E-5);
                        if(fabsf(cl[j]-cr[j]) >  1.0E-6){
                                s = 1;
                                break;
                        }

                }

                if(!s){
                        //      LOG_MSG("Identical!!!");
                        score = 0;
                        num_l = 0;
                        num_r = 0;
                        sl[num_l] = samples[0];
                        num_l++;

                        for(i =1 ; i <num_samples;i++){
                                sr[num_r] = samples[i];
                                num_r++;
                        }



                        /*_mm_free(wr);
                          _mm_free(wl);
                          _mm_free(cr);
                          _mm_free(cl);

                          MFREE(samples);
                          n = alloc_node();
                          //n->left = alloc_node();
                          RUNP(n->left = bisecting_kmeans(msa,n->left, dm, sl, numseq, num_anchors, num_l,rng));
                          //n->right = alloc_node();
                          RUNP(n->right = bisecting_kmeans(msa,n->right, dm, sr, numseq, num_anchors, num_r,rng));
                          return n;*/
                }else{

                        //LOG_MSG("Start run");
                        w = NULL;
                        while(1){
                                stop++;
                                if(stop == 10000){
                                        ERROR_MSG("Failed.");
                                }
                                num_l = 0;
                                num_r = 0;

                                for(i = 0; i < num_anchors;i++){

                                        wr[i] = 0.0f;
                                        wl[i] = 0.0f;
                                }
                                /*fprintf(stdout,"\ncentroids:\n");

                                  for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",cl[j]);

                                  }
                                  fprintf(stdout,"\n");
                                  for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",cr[j]);

                                  }
                                  fprintf(stdout,"\n");
                                  fprintf(stdout,"\n");*/
                                score = 0.0f;
                                for(i = 0; i < num_samples;i++){
                                        s = samples[i];
#ifdef HAVE_AVX2
                                        edist_256(dm[s], cl, num_anchors, &dl);
                                        edist_256(dm[s], cr, num_anchors, &dr);
#else
                                        edist_serial(dm[s], cl, num_anchors, &dl);
                                        edist_serial(dm[s], cr, num_anchors, &dr);
#endif
                                        score += MACRO_MIN(dl,dr);
                                        //fprintf(stdout,"Dist: %f %f\n",dl,dr);
                                        if(dr < dl){
                                                w = wr;
                                                sr[num_r] = s;
                                                num_r++;
                                        }else{
                                                w = wl;
                                                sl[num_l] = s;
                                                num_l++;
                                        }

                                        for(j = 0; j < num_anchors;j++){
                                                w[j] += dm[s][j];
                                        }

                                }
                                //LOG_MSG("%d\tscore: %f",t_iter, score);
                                //LOG_MSG("%d %d", num_l,num_r);
                                for(j = 0; j < num_anchors;j++){
                                        wl[j] /= num_l;
                                        wr[j] /= num_r;
                                }
                                //fprintf(stdout,"\nLeft:\n");

                                /*for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",wl[j]);

                                  }
                                  fprintf(stdout,"\n");
                                  for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",cl[j]);

                                  }
                                  fprintf(stdout,"\n");
                                  fprintf(stdout,"\nRight:\n");
                                  for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",wr[j]);

                                  }
                                  fprintf(stdout,"\n");
                                  for(j = 0; j < num_anchors;j++){
                                  fprintf(stdout,"%f ",cr[j]);

                                  }
                                  fprintf(stdout,"\n");
                                */
                                s = 0;
                                for(j = 0; j < num_anchors;j++){
                                        if(wl[j] != cl[j]){
                                                s = 1;
                                                break;
                                        }
                                        if(wr[j] != cr[j]){
                                                s = 1;
                                                break;

                                        }
                                }
                                if(s){

                                        w = cl;
                                        cl = wl;
                                        wl = w;

                                        w = cr;
                                        cr = wr;
                                        wr = w;
                                }else{
                                        //LOG_MSG("FINISHED");
                                        break;
                                }
                        }
                }
                res_tmp->nl =  num_l;
                res_tmp->nr =  num_r;
                res_tmp->score = score;

                //LOG_MSG("%d\t%f\t%f", t_iter,  res_tmp->score,best->score);
                if(res_tmp->score < best->score){
                        //LOG_MSG("New best: %f", best->score);
                        res_ptr = res_tmp;
                        res_tmp = best;
                        best = res_ptr;
                }
        }
        /*fprintf(stdout,"Samples left (%d):\n", num_l);
        for(i = 0; i < num_l;i++){
                fprintf(stdout,"%d ",sl[i]);
        }
        fprintf(stdout,"\n");

        fprintf(stdout,"Samples right (%d):\n", num_r);
        for(i = 0; i < num_r;i++){
                fprintf(stdout,"%d ",sr[i]);
        }
        fprintf(stdout,"\n");*/
        free_kmeans_results(res_tmp);

        sl = best->sl;
        sr = best->sr;

        num_l = best->nl;

        num_r = best->nr;

        MFREE(best);

        _mm_free(wr);
        _mm_free(wl);
        _mm_free(cr);
        _mm_free(cl);
        MFREE(samples);
        n = alloc_node();
        //n->left = alloc_node();
        RUNP(n->left = bisecting_kmeans(msa,n->left, dm, sl, numseq, num_anchors, num_l,rng));
        //n->right = alloc_node();
        RUNP(n->right = bisecting_kmeans(msa,n->right, dm, sr, numseq, num_anchors, num_r,rng));

        return n;
ERROR:
        return NULL;
}



struct node* upgma(float **dm,int* samples, int numseq)
{
        struct node** tree = NULL;
        struct node* tmp = NULL;

        int i,j;
        int *as = NULL;
        float max;
        int node_a = 0;
        int node_b = 0;
        int cnode = numseq;
        int numprofiles;


        numprofiles = (numseq << 1) - 1;

        MMALLOC(as,sizeof(int)*numseq);
        for (i = numseq; i--;){
                as[i] = i+1;
        }

        MMALLOC(tree,sizeof(struct node*)*numseq);
        for (i = 0;i < numseq;i++){
                tree[i] = NULL;
                tree[i] = alloc_node();
                tree[i]->id = samples[i];
        }

        while (cnode != numprofiles){
                max = FLT_MAX;
                for (i = 0;i < numseq-1; i++){
                        if (as[i]){
                                for ( j = i + 1;j < numseq;j++){
                                        if (as[j]){
                                                if (dm[i][j] < max){
                                                        max = dm[i][j];
                                                        node_a = i;
                                                        node_b = j;
                                                }
                                        }
                                }
                        }
                }
                tmp = NULL;
                tmp = alloc_node();
                tmp->left = tree[node_a];
                tmp->right = tree[node_b];


                tree[node_a] = tmp;
                tree[node_b] = NULL;

                /*deactivate  sequences to be joined*/
                as[node_a] = cnode+1;
                as[node_b] = 0;
                cnode++;

                /*calculate new distances*/
                for (j = numseq;j--;){
                        if (j != node_b){
                                dm[node_a][j] = (dm[node_a][j] + dm[node_b][j])*0.5f;
                                //dm[node_a][j] = MACRO_MAX(dm[node_a][j],dm[node_b][j]);
                                //dm[node_a][j] = MACRO_MIN(dm[node_a][j],dm[node_b][j]);
                        }
                }
                dm[node_a][node_a] = 0.0f;
                for (j = numseq;j--;){
                        dm[j][node_a] = dm[node_a][j];
                        //           dm[j][node_b] = 0.0f;
                        //dm[node_b][j] = 0.0f;
                }
        }
        tmp = tree[node_a];
        MFREE(tree);
        MFREE(as);
        return tmp;
ERROR:
        return NULL;
}

struct node* alloc_node(void)
{
        struct node* n = NULL;
        MMALLOC(n, sizeof(struct node));
        n->left = NULL;
        n->right = NULL;
        n->id = -1;
        return n;
ERROR:
        return NULL;
}



int label_internal(struct node*n, int label)
{
        if(n->left){
                label = label_internal(n->left, label);
        }
        if(n->right){
                label = label_internal(n->right, label);
        }

        if(n->id == -1){
                n->id = label;
                label++;
        }
        return label;

}

int* readbitree(struct node* p,int* tree)
{
        if(p->left){
                tree = readbitree(p->left,tree);
        }
        if(p->right){
                tree = readbitree(p->right,tree);
        }

        if(p->left){
                if(p->right){
                        tree[tree[0]] = p->left->id;
                        tree[tree[0]+1] = p->right->id;
                        tree[tree[0]+2] = p->id;
                        tree[0] +=3;
                        MFREE(p->left);
                        MFREE(p->right);
                }
        }
        return tree;
}


struct kmeans_result* alloc_kmeans_result(int num_samples)
{
        struct kmeans_result* k = NULL;
        ASSERT(num_samples!= 0, "No samples???");


        MMALLOC(k, sizeof(struct kmeans_result));

        k->nl = 0;
        k->nr = 0;
        k->sl = NULL;
        k->sr = NULL;
        MMALLOC(k->sl, sizeof(int) * num_samples);
        MMALLOC(k->sr, sizeof(int) * num_samples);
        k->score = FLT_MAX;
        return k;
ERROR:
        free_kmeans_results(k);
        return NULL;
}

void free_kmeans_results(struct kmeans_result* k)
{
        if(k){
                if(k->sl){
                        MFREE(k->sl);
                }
                if(k->sr){
                        MFREE(k->sr);
                }
                MFREE(k);
        }
}

/*
int rec[1000006];
void printTree(struct node* curr,int depth)
{
        int i;
        if(curr==NULL)return;
        printf("\t");
        for(i=0;i<depth;i++){
                if(i==depth-1){
                        printf("%s\u2014\u2014\u2014",rec[depth-1]?"\u0371":"\u221F");
                }else{
                        printf("%s   ",rec[i]?"\u23B8":"  ");
                }
        }
        printf("%d\n",curr->id);
        rec[depth]=1;
        printTree(curr->left,depth+1);
        rec[depth]=0;
        printTree(curr->right,depth+1);
}
*/
