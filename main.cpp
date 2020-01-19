#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <assert.h>
#include <set>

using namespace std;

void printv(vector<int> v)
{
    cout << "v(" << v.size() << "): ";
    for (unsigned i = 0; i < v.size(); i++)
        cout << v[i] << " ";
    cout << endl;
}

vector<int> makeTest(int f, int failed = 0)
{
    cout << "making test (f=" << f << "; failed=" << failed << ")" << endl;
    int N = 3 * f + 1;

    vector<int> choices(N, -1);

    int count_primaries = 0;
    // must have at least one primary alive (non-faulty)
    while (count_primaries == 0)
    {
        count_primaries = 2; // two primaries (must have at least one alive)
        // double proposal
        choices[0] = 0;
        choices[1] = 1;
        // set failed nodes
        while (failed > 0)
        {
            int i = rand() % N; // failed
            if (choices[i] != -2)
            {
                choices[i] = -2;
                cout << " --> failed " << i << endl;
                failed--;
                if ((i == 0) || (i == 1))
                {
                    cout << "-----> lost primary!" << endl;
                    count_primaries--;
                }
            }
        }
    }

    // set rest of nodes first responses
    for (unsigned i = 2; i < N; i++)
    {
        int c = choices[i];
        while (c == -1) // not selected yet
        {
            c = rand() % 2;       // double proposal
            if (choices[c] == -2) // re-propose because it's faulty
            {
                c = -1;
                continue;
            }
        }
        choices[i] = c;
    }

    return choices;
}

// select 2f random responses for i (non-faulty choices)
vector<int> make2f(vector<int> choices, int f, int i)
{
    int N = 3 * f + 1;
    vector<int> options;
    for (unsigned k = 0; k < N; k++)
        if ((k != i) && (choices[k] != -2) && (k != choices[i])) // not i and non-faulty
                                                                 // (last is to avoid repetition of response): R_2 | 0(0) |    0(2)    0(0)    | Cancel 0
            options.push_back(k);
    std::random_shuffle(options.begin(), options.end());
    // must add i as first on its list, if not proposer
    if (i > 1)                              // non-primary {0,1}
        options.insert(options.begin(), i); // insert first prepare response (choice)
    while (options.size() > 2 * f)
        options.pop_back();
    // must verify that, if responded to a second request, it's relaying first its response, than request itself (priority)
    // example: I answer to Prop1 first, then I receive Prop0... If I decide to also respond to it, I may prefer the response, than the request (it's more info)
    // perhaps a problem o logic in this method here... suppose I'm 2.. if I had choice 1, used 2 to respond to 1.. I cannot respond to 0 (2 was used already). Must do this outside this method.
    // TODO: must improve this, change this method signature with more precise info (value/origin)
    // This is the bug: R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
    // Should respond to zero first with my index, than just prepare request

    return options;
}

vector<vector<pair<int, int>>> selections(vector<int> choices, int f)
{
    int N = 3 * f + 1;
    vector<vector<pair<int, int>>> sel(N, vector<pair<int, int>>(2 * f, make_pair(-2, -2)));
    // for each i in N, select possible 2f responses, in order
    for (int i = 0; i < N; i++)
    {
        if (choices[i] == -2) // faulty node
            continue;
        vector<int> choicesi = make2f(choices, f, i); // 2f responses for replica i
        for (unsigned j = 0; j < choicesi.size(); j++)
            sel[i][j] = make_pair(choicesi[j], choices[choicesi[j]]);
        // may have bug here
        // R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
        // we must prefer response over request, if double response being made
        // I think this only applies to non-prepare nodes
        if (i > 0) // only backups (or primary 1) ... not primary zero
        {
            int prepZero = -1;
            int respZero = -1;
            int prepOne = -1;
            int respOne = -1;
            for (unsigned k = 0; k < sel[i].size(); k++)
            {
                if ((sel[i][k].second == 0) && (sel[i][k].first == 0)) // prepareZero
                    prepZero = k;
                if ((sel[i][k].second == 1) && (sel[i][k].first == 1)) // prepareOne
                    prepOne = k;
                if ((sel[i][k].second == 0) && (sel[i][k].first == i)) // respZero
                    respZero = k;
                if ((sel[i][k].second == 1) && (sel[i][k].first == i)) // respOne
                    respOne = k;
            }
            cout << "*** i=" << i << " | choice=" << choices[i] << " | ";
            for (unsigned k = 0; k < sel[i].size(); k++)
                cout << sel[i][k].second << "(" << sel[i][k].first << ") ";
            cout << endl;

            // disabling multiple response
            /*
            if (choices[i] == 0) // assert that my prepare and response is going out
            {
                //assert(prepZero>-1); // this is my choice, not in this vector (TODO: improve)
                assert(respZero > -1);
                // if re-proposing, must prioritize reply
                if ((prepOne > -1) && (respOne == -1))
                {
                    // propose but no reply, let's change
                    //sel[i][prepOne].first = i; // now I'm responding, not sending proposal only
                    sel[i].push_back(make_pair(i, 1)); // My response
                    //assert(respOne > -1); // THIS must be valid
                }
            }
            if (choices[i] == 1) // assert that my prepare and response is going out
            {
                //assert(prepOne>-1); // this is my choice, not in this vector (TODO: improve)
                if(i != 1) // 'one' does not respond to itself
                    assert(respOne > -1);
                // if re-proposing, must prioritize reply
                if ((prepZero > -1) && (respZero == -1))
                {
                    // propose but no reply, let's change
                    //sel[i][prepZero].first = i; // now I'm responding, not sending proposal only
                    sel[i].push_back(make_pair(i, 0)); // My response
                    //assert(respZero > -1); // THIS must be valid
                }
            }
            */
            // DISABLED: this generates forks

        } // backups
    }

    return sel;
}

vector<int> getCancels(vector<int> choices, vector<vector<pair<int, int>>> selections, int f)
{
    int N = 3 * f + 1;
    vector<int> cancels(N, -2);
    // compute cancels for each replica
    // OLD // rule is: order by lowest to highest proposal, and take value 2*f
    // NEW RULE (simpler): if Prep0 has a non-faulty proposal, Cancel 0, otherwise Cancel 1
    for (unsigned i = 0; i < N; i++)
    {
        if (choices[i] == -2)
            continue;       // faulty node
        int count_zero = 0; // try to Cancel 0 (NEW RULE)
        //vector<int> responses;
        //responses.push_back(choices[i]); // own choice counts
        //for (unsigned j = 0; j < selections[i].size(); j++)
        //    responses.push_back(selections[i][j].second);
        ////cout << "RESPONSES FOR I=" << i << endl;
        ////printv(responses);
        //sort(responses.begin(), responses.end());
        //printv(responses);
        //int cancel = responses[2 * f - 1];
        for (unsigned j = 0; j < selections[i].size(); j++)
            if (selections[i][j].second == 0)
                count_zero++;
        if (choices[i] == 0)
            count_zero++;
        int cancel = 1;          // default one
        if (count_zero >= f + 1) // at least one trusted
            cancel = 0;

        ////cout << "cancel = " << cancel << endl;
        cancels[i] = cancel;
        // EXCEPTION: proposal j cannot Cancel more than j (otherwise it just assumes its "faultyness", what is absurd)
        if (i == 0)
            cancels[i] = 0; // FORCE. could do same for i=1 (but only two, so that's fine already)
    }

    return cancels;
}

void printTable(vector<int> choices, vector<vector<pair<int, int>>> selections, vector<int> cancels, int f)
{
    int N = 3 * f + 1;
    for (unsigned i = 0; i < N; i++)
    {
        cout << "R_" << i << " | " << choices[i] << "(" << choices[i] << ") |\t";
        for (unsigned j = 0; j < selections[i].size(); j++)
            cout << selections[i][j].second << "(" << selections[i][j].first << ")\t";
        cout << "| Cancel " << cancels[i] << endl;
    }
}

/*
int getCommitCountFromCancels(vector<int> cancels, vector<vector<pair<int, int>>> _unused, int f)
{
    // compute commit counts
    // commit zero (X_0) may be issued when any zero is present
    // commit one (X_1) may be issued when 2*f+1 ones is present
    int commits = 0;
    // look for zero commits
    for (unsigned i = 0; i < cancels.size(); i++)
        if (cancels[i] == 0)
        {
            commits++; // found X_0
            break;
        }
    // look for one commits
    int ones = 0;
    for (unsigned i = 0; i < cancels.size(); i++)
        if (cancels[i] == 1)
            ones++;
    if (ones >= 2 * f + 1) // found a possible X_1
        commits++;

    return commits;
}
*/

struct CommitStatus
{
    CommitStatus(int _commit, bool _undecided = false, bool _dead = false, int _msgCount = -1) : commit(_commit), undecided(_undecided), dead(_dead), msgCount(_msgCount)
    {
    }

    int commit;     // commit id
    bool undecided; // not enough info
    bool dead;      // dead node in subset
    int msgCount;   // message count
};

vector<int> verifyFastCommits(vector<int> cancels, vector<int> choices, vector<vector<pair<int, int>>> selections)
{
    int N = choices.size();
    int f = (N - 1) / 3;
    vector<int> fast(N, -2);
    for (unsigned i = 0; i < N; i++)
    {
        int sum_zeros = 0;
        int sum_ones = 0;
        if (choices[i] != -2) // exists
        {
            if (cancels[i] == 0)
                sum_zeros++;
            if (cancels[i] == 1)
                sum_ones++;
            for (unsigned k = 0; k < selections[i].size(); k++)
            {
                if (selections[i][k].second == 0)
                    sum_zeros++;
                if (selections[i][k].second == 1)
                    sum_ones++;
            }
        }
        // compute fast commits
        if (sum_ones >= 2 * f + 1) // fast
            fast[i] = 1;
        if (sum_zeros >= f + 1) // fast
            fast[i] = 0;
    }
    return fast;
}

// p = N = 3f+1.   k = 2f+1
CommitStatus verifyCommitSubset(vector<int> cancels, vector<int> choices, vector<vector<pair<int, int>>> selections, int t[], int p, int k)
{
    int N = p;
    int f = (N - 1) / 3;
    //vector<pair<int, int>> pack; // pack all information
    std::set<pair<int, int>> pack;
    int count_cancel_zero = 0;
    int count_cancel_one = 0;
    //bool includes_primaryZero = false;
    for (unsigned c = 0; c < k; c++)
    {
        int e = t[c]; // get element from subset
        if (cancels[e] == -2)
            return CommitStatus{-4, false, true}; // dead node
        if (cancels[e] == 0)
            count_cancel_zero++;
        if (cancels[e] == 1)
            count_cancel_one++;
        /*
        if(e == 0)
        {
            includes_primaryZero = true;
            return 0; // very strong primary 0
        }
        */
        pack.insert(make_pair(choices[e], choices[e])); // first proposal (responded to)
        for (unsigned j = 0; j < selections[e].size(); j++)
            pack.insert(selections[e][j]); // responses (and possible more proposals)
    }

    cout << "PACK for p=N=" << p << " 2f+1=k=" << k << endl;
    /*
    sort(pack.begin(), pack.end(), [](const pair<int,int> & a, const pair<int,int> & b) -> bool
    { 
        return (a.first < b.first) && (a.second < b.second); 
    });
    unique(pack.begin(), pack.end());
    */
    //sort(pack.begin(), pack.end());
    //pack.erase(unique(pack.begin(), pack.end()));
    // remove repeated
    //for(unsigned i=0; i<int(pack.size())-1; i++)
    //    for(unsigned j=i+1; j<pack.size(); j++)

    //for(int j=0; j<pack.size(); j++)
    //    cout << pack[j].second << "(" << pack[j].first << ")" << "\t";
    int count_zero = 0;
    int count_one = 0;
    for (auto dp : pack)
    {
        cout << dp.second << "(" << dp.first << ")"
             << "\t";
        if (dp.second == 0)
            count_zero++;
        if (dp.second == 1)
            count_one++;
    }
    cout << endl;
    cout << "count_zero = " << count_zero << endl;
    cout << "count_one = " << count_one << endl;
    //if(count_zero >= 2) // f+1
    //    return 0;
    //if(count_zero >= k) // >=2f+1
    if (count_one >= 2 * f + 1) // >=2f+1 // this is correct one
                                //if (count_one >= f+1) // >=f+1 // NOT GOOD!! JUST TESTING (INTENDS TO FAIL AND CREATE FORKS!!)
        return 1;
    if (count_zero >= f + 1) // >=f+1  // this is correct one
        return 0;

    // THINK, WHAT ELSE?
    //return 0; // default zero (seems to cause problem, when all is 1, an intersection doesnt give 2f+1 ones...)
    // probably, voting.

    // VERY DUBIOUS CASE HERE... 0(0)    1(1)    1(3)
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    0(0)    1(3)    | Cancel 1
R_2 | 0(0) |    0(2)    1(1)    | Cancel 0
R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
    */
    // but it should be zero.

    // try to vote by cancels.... desperate measure
    //if(count_cancel_zero > 0)
    //    return 0;
    //else
    //    return 1;
    // didn't work out...
    // all ones and only first is zero. No one answered it. So, it must go one.
    //if((count_one == 2) && (count_zero==1))
    //    return 1;
    // default
    //return 0;

    // cannot decide (THIS IS THE SOLUTION!!!)
    // sometimes, one should simply wait for more information...
    // ... and if really not possible, then Change View (rare cases, only 12.5% with permanent fault)
    return CommitStatus{-4, true};

    /*
    // didn't work out... dubious case:  0(0)    1(1)    1(3)
    if(count_one > count_zero)
        return 1;
    else
        return 0; // equality, still zero
        */
}

void getCommitSubsets(int &countZero, int &countOne, int &countHang, vector<int> cancels, vector<int> choices, vector<vector<pair<int, int>>> selections, int s[], int p, int k, int t[], int q = 0, int r = 0)
{
    // all subsets of size k, from p elements in total (N)
    if (q == k)
    {
        cout << "verifying subset pack:" << endl;
        for (int i = 0; i < k; i++)
            printf("%d ", t[i]);
        printf("\n");
        CommitStatus _com = verifyCommitSubset(cancels, choices, selections, t, p, k);
        int commit = -4;
        if (_com.undecided)
            commit = -3;
        else if (_com.dead)
            commit = -2;
        else
            commit = _com.commit;
        cout << "RETURNED COMMIT = " << commit << endl
             << endl;
        if (commit == 0)
            countZero++;
        else if (commit == 1)
            countOne++;
        else if (commit == -3)
            countHang++; // cannot decide, must wait
        else if (commit == -2)
        {
            cout << "DEAD NODE IN SUBSET!" << endl;
            cout << "IGNORING..." << endl;
        }
        else
        {
            cout << "UNDEFINED COMMIT!" << endl;
            exit(1);
        }
    }
    else
    {
        for (int i = r; i < p; i++)
        {
            t[q] = s[i];
            getCommitSubsets(countZero, countOne, countHang, cancels, choices, selections, s, p, k, t, q + 1, i + 1);
        }
    }
}

// 'selections' is only used for dubious commit 0 case (on primary 0)
int getCommitCountFromCancels(vector<int> cancels, vector<vector<pair<int, int>>> selections, int f)
{
    // compute commit counts
    // commit zero (X_0) may be issued when any zero is present
    // commit one (X_1) may be issued when 2*f+1 ones is present

    bool hasCommitOne = false;
    bool hasCommitZero = false; // a little bit more complex

    bool hasBadZero = false;     // bad zero condition, may help commit one too... BadZero + 2 ones (N=4).. when 1 is faulty
    bool hasTrustedZero = false; // trusted zero should be the same as BadZero above...

    // look for zero commits
    for (unsigned i = 0; i < cancels.size(); i++)
    {
        // if not cancel zero, proceed...
        if (cancels[i] != 0)
            continue;

        // found POSSIBLE X_0

        // must verify primary edge case
        if (i == 0) // may be tricky (mandatory for replica 0 to issue cancel 0)
        {
            // if primary 0 has another independent confirmation, then it's good
            int confirmations = 0;
            // selections contains only prepare responses (not request!)
            for (unsigned k = 0; k < selections[0].size(); k++)
                if ((selections[0][k].second == 0)) //&& (selections[0][k].first != 0)) // a zero from someone else
                    confirmations++;                // including itself
            if (confirmations >= 1)                 // more than itself
            {
                hasCommitZero = true;
                break; // guaranteed
            }
            else
            {
                // only itself...
                hasTrustedZero = true;
                // one may received this "Cancel 0" with two other Cancel 1 (for 2f+1 = 3)
                // must find in ALL other possible Cancel 1, if it could provide evidence that ONE NON-FAULTY zero exists.
                // let's go.
                int countConfirmed = 0; // must have at least 2f OTHERS including a VALID zero (a non-faulty zero)
                for (unsigned j = 1; j < cancels.size(); j++)
                {
                    if (cancels[j] == 0) // OK, if you are lucky to receive this one...
                        countConfirmed++;
                    else
                    {
                        // not so lucky this time...
                        if (cancels[j] == 1) // any good option must include extra f+1
                        {
                            for (unsigned k = 0; k < selections[j].size(); k++)
                                if ((selections[j][k].second == 0) && (selections[j][k].first != 0))
                                {
                                    countConfirmed++;
                                    break;
                                }
                        }
                    }
                } // for j=1 to N-1

                // one needs this zero + 2f good ones (TODO: confirm for 7 nodes... maybe this is N-2... for 4 its same as 2f+1.. who knows)
                if (countConfirmed >= 2 * f)
                    hasCommitZero = true;
                else
                    hasBadZero = true; // bad zero may help commit one, perhaps!
            }
        }                         // end replica 0 for cancel 0
        else                      // this is trivial, not primary, thus certain value.
            hasCommitZero = true; // this one is certainly good

        // if found zero already, breaks search
        if (hasCommitZero)
            break;
    } // end looking for zeros

    // look for one commits
    int ones = 0;
    for (unsigned i = 0; i < cancels.size(); i++)
        if (cancels[i] == 1)
            ones++;
    if (ones >= 2 * f + 1) // found a possible X_1
        hasCommitOne = true;

    // special case: two one's and one BadZero (N=4) and a failed node...
    if (ones + hasBadZero == 2 * f + 1) // since ones are never in primary when BadZero is, this is good.
        hasCommitOne = true;

    cout << "trustedzero: " << hasTrustedZero << endl;
    cout << "badzero: " << hasBadZero << endl;
    cout << "commit0: " << hasCommitZero << endl;
    cout << "commit1: " << hasCommitOne << endl;

    return hasCommitZero + hasCommitOne;
}

/*
void g(int s[],int p,int k,int t[],int q=0,int r=0)
{
    if(q==k)
    {
        for(int i=0;i<k;i++)
            printf("%d ",t[i]);
        printf("\n");
    }
    else
    {
        for(int i=r;i<p;i++)
        {
            t[q]=s[i];
            g(s,p,k,t,q+1,i+1);
        }
    }
}
*/
// https://stackoverflow.com/questions/4555565/generate-all-subsets-of-size-k-from-a-set
//void main_subset() {
//int s[]={1,2,3,4,5,6,7},t[7];
//g(s,7,5,t);
//}

// MUST CLEARIFY THE LOGIC
// Cancel Phase may perhaps be a Pre-Commit phase (if "cancel" itself has no meaning... let's see)
// if I receive 2f+1 valid0, then Cancel0 -> Commit0 (0 is surely the way to go)
// if I receive 2f+1 valid1, then Cancel1. But not yet sure to commit1 directly... should wait for others (perhaps it's just me. Verify this logic!!)
// if I receive 2f+1 Cancels, analyse content (aggregate). If 2f+1 zeros, it's zero.  If 2f+1 ones, it's one (check this, I hope so!). Otherwise, it's zero (check this!).

int main()
{
    // Solution to current problems:
    // see paper version, and fix fact that responses must be included as mandatory
    // this is not good:
    /*
R_0 | 0 |       0(3)    1(1)    | Cancel 0
R_1 | 1 |       1(2)    0(3)    | Cancel 1
R_2 | 1 |       0(0)    1(1)    | Cancel 1
R_3 | 0 |       1(1)    1(2)    | Cancel 1
possible commits: 2
    */
    // Note that R_3 must include itself on list, that's mandatory

    // This was "probably" fixed. Now, what's this issue?
    /*
R_0 | 0 |       1(1)    1(3)    | Cancel 1  -> should be 0 (otherwise it just assumes its "faultyness")
R_1 | 1 |       0(2)    1(3)    | Cancel 1
R_2 | 0 |       0(2)    1(3)    | Cancel 0
R_3 | 1 |       1(3)    0(2)    | Cancel 1
possible commits: 2
*/
    // As I imagined, this causes another problem (forcing 0 to issue Cancel 0). We must make sure there are f+1 (one non-faulty zero, to issue a commit 0)
    /*
R_0 | 0 |       1(2)    1(1)    | Cancel 0
R_1 | 1 |       0(0)    1(3)    | Cancel 1
R_2 | 1 |       1(2)    1(1)    | Cancel 1
R_3 | 1 |       1(3)    1(2)    | Cancel 1
possible commits: 2
*/
    // Looks like some error in logic
    /*
R_0 | 0 |       1(3)    1(1)    | Cancel 0
R_1 | 1 |       0(2)    0(0)    | Cancel 0
R_2 | 0 |       0(2)    0(0)    | Cancel 0
R_3 | 1 |       1(3)    1(1)    | Cancel 1
possible commits: 0
SPORK! Multiple or Zero commits
*/
    // ONE MORE....
    /*
R_0 | 0 |       1(1)    1(3)    | Cancel 0
R_1 | 1 |       1(3)    0(0)    | Cancel 1
R_2 | -2 |      -2(-2)  -2(-2)  | Cancel -2
R_3 | 1 |       1(3)    1(1)    | Cancel 1
commit0: 0
commit1: 0
possible commits: 0
SPORK! Multiple or Zero commits
*/
    // meeting the devil in person
    /*
R_0 | 0 |       1(1)    1(3)    | Cancel 0
R_1 | 1 |       0(0)    1(3)    | Cancel 1
R_2 | 0 |       0(2)    0(0)    | Cancel 0
R_3 | 1 |       1(3)    0(0)    | Cancel 1
trustedzero: 1
badzero: 1
commit0: 1
commit1: 1
possible commits: 2
SPORK! Multiple or Zero commits
*/
    // MAYBE... reason is: only push 0, if all 2f+1 have proposal 0 on them, or a single non-faulty 0.
    // This means all 2f+1 know about 0 at least, if cannot guarantee a non-faulty zero.
    // TODO: try this... but first, look for bug.
    // interpretation of 0(0) on other nodes, may be a re-response (thus enforcing its validity)
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    0(0)    1(3)    | Cancel 1
R_2 | 0(0) |    0(2)    1(1)    | Cancel 0
R_3 | 1(1) |    1(3)    0(0)    | Cancel 1    <----------- strange
trustedzero: 1
badzero: 1
commit0: 1
commit1: 1
possible commits: 2
*/
    // Now, the strangest one....
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    1(3)    1(2)    | Cancel 1
R_2 | 1(1) |    1(2)    0(2)    | Cancel 1
R_3 | 1(1) |    1(3)    0(3)    | Cancel 1
trustedzero: 1
badzero: 0
commit0: 1
commit1: 1
possible commits: 2
SPORK! Multiple or Zero commits
*/

    // Still bug
    /*
R_0 | 0(0) |    1(3)    1(2)    | Cancel 0
R_1 | 1(1) |    1(2)    0(0)    | Cancel 1   <------------- Strange
R_2 | 1(1) |    1(2)    0(0)    0(2)    | Cancel 0
R_3 | 1(1) |    1(3)    1(2)    | Cancel 1
trustedzero: 1
badzero: 1
commit0: 1
commit1: 1
possible commits: 2
SPORK! Multiple or Zero commits
*/

    // maybe try to simplify logic again (only cancels)
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    1(3)    0(0)    0(1)    | Cancel 0
R_2 | 1(1) |    1(2)    1(3)    | Cancel 1
R_3 | 1(1) |    1(3)    1(2)    | Cancel 1
trustedzero: 1
badzero: 1
commit0: 1
commit1: 1
possible commits: 2
SPORK! Multiple or Zero commits
*/

    // Working with subsets now... this case is also very strange
    /*
*** i=3 | choice=1 | 1(3) 0(0) 
R_0 | 0(0) |    1(1)    1(2)    | Cancel 0
R_1 | 1(1) |    0(0)    1(2)    | Cancel 1
R_2 | 1(1) |    1(2)    0(0)    | Cancel 1
R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
verifying subset pack:
0 1 2 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(2)             <------------- Too few information to decide... crazy!
count_zero = 1
count_one = 2
RETURNED COMMIT = 0                   

verifying subset pack:
0 1 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(2)    1(3)
count_zero = 1
count_one = 3
RETURNED COMMIT = 1

verifying subset pack:
0 2 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(2)    1(3)
count_zero = 1
count_one = 3
RETURNED COMMIT = 1

verifying subset pack:
1 2 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(2)    1(3)
count_zero = 1
count_one = 3
RETURNED COMMIT = 1

countZero = 1
countOne = 3
possible commits: 2
SPORK! Multiple or Zero commits
*/

    // Perhaps the HARDEST one.
    // Voting may resolve the up one (that looks simpler...). But now this one, looks hard again.
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    0(0)    1(3)    | Cancel 1
R_2 | 0(0) |    0(2)    1(1)    | Cancel 0
R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
verifying subset pack:
0 1 2 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    0(2)    1(3)
count_zero = 2
count_one = 2
RETURNED COMMIT = 0

verifying subset pack:
0 1 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(3)
count_zero = 1
count_one = 2
RETURNED COMMIT = 1

verifying subset pack:
0 2 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    0(2)    1(3)
count_zero = 2
count_one = 2
RETURNED COMMIT = 0

verifying subset pack:
1 2 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    0(2)    1(3)
count_zero = 2
count_one = 2
RETURNED COMMIT = 0

countZero = 3
countOne = 1
possible commits: 2
SPORK! Multiple or Zero commits
*/

    // very BEAUTIFUL hang option, when no known choice is available
    /*
R_0 | 0(0) |    1(1)    1(3)    | Cancel 0
R_1 | 1(1) |    0(0)    1(3)    | Cancel 1
R_2 | -2(-2) |  -2(-2)  -2(-2)  | Cancel -2
R_3 | 1(1) |    1(3)    0(0)    | Cancel 1
verifying subset pack:
0 1 2 
RETURNED COMMIT = -2

DEAD NODE IN SUBSET!
IGNORING...
verifying subset pack:
0 1 3 
PACK for p=N=4 2f+1=k=3
0(0)    1(1)    1(3)
count_zero = 1
count_one = 2
RETURNED COMMIT = -3

verifying subset pack:
0 2 3 
RETURNED COMMIT = -2

DEAD NODE IN SUBSET!
IGNORING...
verifying subset pack:
1 2 3 
RETURNED COMMIT = -2

DEAD NODE IN SUBSET!
IGNORING...
countZero = 0
countOne = 0
countHang = 1
possible commits: 0
SPORK! Multiple or Zero commits
*/

    //main_subset();
    //exit(1);
    cout << "======== begin tests ========" << endl;
    //srand(time(NULL));
    srand(0);

    //int NUM_TESTS = 10000;
    //int NUM_TESTS = 1000000;
    int NUM_TESTS = 1000; // reduced to 1k (quick)

    // ===================||
    //                    ||
    //                    ||
    //                   \\//
    //
    // =====================================
    // ONLY THING TO CHANGE IS THE 'f' BELOW
    // =====================================
    int f = 1; // N = 4
    //int f = 2; // N = 7
    //int f = 3; // N = 10

    // summary stuff
    int countHangsChangeView = 0;
    int countGlobalCommit0 = 0;
    int countGlobalCommit1 = 0;
    int countGlobalUndecided = 0;
    int countCommit0 = 0;
    int countCommit1 = 0;

    int countSizeGlobal = 0;
    int countFastCommits = 0;
    for (unsigned test = 0; test < NUM_TESTS; test++)
    {
        int N = 3 * f + 1;
        //int s[] = {0, 1, 2, 3, 4, 5, 6, 7}, t[N];
        int s[N], t[N];
        for (unsigned i = 0; i < N; i++)
            s[i] = i; // int s[] = {0, 1, 2, 3}

        cout << endl;
        cout << "TEST: " << test << endl;

        int faulty = rand() % (f + 1); // 0..f faulty nodes

        //vector<int> vec = makeTest(f, 0); // 0 buggy
        //vector<int> vec = makeTest(f, 1); // 1 buggy
        vector<int> choices = makeTest(f, faulty);
        printv(choices);

        vector<int> choicesi0 = make2f(choices, f, 0); // responses for replica 0
        printv(choicesi0);

        vector<vector<pair<int, int>>> sel = selections(choices, f);

        vector<int> cancels = getCancels(choices, sel, f);

        printTable(choices, sel, cancels, f);

        int countZero = 0;
        int countOne = 0;
        int countHang = 0;

        getCommitSubsets(countZero, countOne, countHang, cancels, choices, sel, s, 3 * f + 1, 2 * f + 1, t);

        //printv(cancels);
        cout << "countZero = " << countZero << endl;
        cout << "countOne = " << countOne << endl;
        cout << "countHang = " << countHang << endl;

        int commits = (countZero > 0) + (countOne > 0);
        cout << "possible commits: " << commits << endl;
        if (faulty == 0) // no faulty, must decide
        {
            if (commits != 1)
            {
                cout << "SPORK!"
                     << " Multiple or Zero commits" << endl;
                exit(1);
            }
        }
        else
        {
            // has faulty
            if (commits > 1)
            {
                cout << "SPORK!"
                     << " Multiple or Zero commits" << endl;
                exit(1);
            }
            if (commits == 0)
            {
                countHangsChangeView++;
                cout << "SYSTEM CHANGES VIEW!" << endl;
            }
        }

        countCommit0 += (countZero > 0);
        countCommit1 += (countOne > 0);

        /*
        int commits = getCommitCountFromCancels(cancels, sel, f);
        cout << "possible commits: " << commits << endl;
        if (commits != 1)
        {
            cout << "SPORK!"
                 << " Multiple or Zero commits" << endl;
            exit(1);
        }
        */

        // compute global
        {
            vector<int> valid;
            for (unsigned i = 0; i < N; i++)
                if (choices[i] != -2)
                    valid.push_back(i);
            int vt[valid.size()];
            for (unsigned i = 0; i < valid.size(); i++)
                vt[i] = valid[i];
            CommitStatus _com = verifyCommitSubset(cancels, choices, sel, vt, N, valid.size());
            cout << "GLOBAL: commit=" << _com.commit << " undecided=" << _com.undecided << endl;
            if (_com.commit == 0)
                countGlobalCommit0++;
            if (_com.commit == 1)
                countGlobalCommit1++;
            if (_com.undecided)
                countGlobalUndecided++;
            countSizeGlobal += valid.size();

            vector<int> fast = verifyFastCommits(cancels, choices, sel);
            for (unsigned k = 0; k < fast.size(); k++)
                if (fast[k] != -2)
                {
                    assert(fast[k] == _com.commit);
                    countFastCommits++;
                }
        }
    }

    cout << " ========= SUMMARY =========" << endl;
    cout << "REPLICAS N=" << 3 * f + 1 << " f=" << f << endl;
    cout << "CHANGE VIEWS = " << countHangsChangeView << " / " << 100 * countHangsChangeView / double(NUM_TESTS) << "%" << endl;
    cout << "COMMIT0 = " << countCommit0 << " / " << 100 * countCommit0 / double(NUM_TESTS) << "%" << endl;
    cout << "COMMIT1 = " << countCommit1 << " / " << 100 * countCommit1 / double(NUM_TESTS) << "%" << endl;
    cout << " ========= GLOBAL =========" << endl;
    cout << "GL.UNDECIDED = " << countGlobalUndecided << " / " << 100 * countGlobalUndecided / double(NUM_TESTS) << "%" << endl;
    cout << "GL.SIZE = (avg) " << countSizeGlobal / double(NUM_TESTS) << endl;
    cout << "COUNT.FAST = (avg) " << countFastCommits / double(NUM_TESTS) << endl;
    cout << "GL.COMMIT0 = " << countGlobalCommit0 << " / " << 100 * countGlobalCommit0 / double(NUM_TESTS) << "%" << endl;
    cout << "GL.COMMIT1 = " << countGlobalCommit1 << " / " << 100 * countGlobalCommit1 / double(NUM_TESTS) << "%" << endl;
    cout << " ==================" << endl;
    cout << "finished successfully" << endl;
    cout << endl;
    return 0;
}