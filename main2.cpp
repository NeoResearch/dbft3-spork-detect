#include <iostream>
#include <algorithm>
#include <vector>
#include <random>

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
            failed--;
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
        if ((k != i) && (choices[k] != -2)) // not i and non-faulty
            options.push_back(k);
    std::random_shuffle(options.begin(), options.end());
    // must add i as first on its list, if not proposer
    if (i > 1) // non-primary {0,1}
        options.insert(options.begin(), i);
    while (options.size() > 2 * f)
        options.pop_back();
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
    }

    return sel;
}

vector<int> getCancels(vector<int> choices, vector<vector<pair<int, int>>> selections, int f)
{
    int N = 3 * f + 1;
    vector<int> cancels(N, -2);
    // compute cancels for each replica
    // rule is: order by lowest to highest proposal, and take value 2*f
    for (unsigned i = 0; i < N; i++)
    {
        if (choices[i] == -2)
            continue; // faulty node
        vector<int> responses;
        responses.push_back(choices[i]); // own choice counts
        bool hasProposal0 = false;
        for (unsigned j = 0; j < selections[i].size(); j++)
        {
            responses.push_back(selections[i][j].second);
            if((selections[i][j].first == 0) && (selections[i][j].second == 0)) // primary proposes 0
                hasProposal0 = true;
        }
        
        //cout << "RESPONSES FOR I=" << i << endl;
        //printv(responses);
        sort(responses.begin(), responses.end());
        //printv(responses);
        int cancel = responses[2 * f - 1];
        //cout << "cancel = " << cancel << endl;
        cancels[i] = cancel;
        // EXCEPTION: proposal j cannot Cancel more than j (otherwise it just assumes its "faultyness", what is absurd)
        if (i == 0)
            cancels[i] = 0; // FORCE. could do same for i=1 (but only two, so that's fine already)
        if(hasProposal0) // in responses get proposal 0, commit to it
            cancels[i] = 0;
    }

    return cancels;
}

void printTable(vector<int> choices, vector<vector<pair<int, int>>> selections, vector<int> cancels, int f)
{
    int N = 3 * f + 1;
    for (unsigned i = 0; i < N; i++)
    {
        cout << "R_" << i << " | " << choices[i] << " |\t";
        for (unsigned j = 0; j < selections[i].size(); j++)
            cout << selections[i][j].second << "(" << selections[i][j].first << ")\t";
        cout << "| Cancel " << cancels[i] << endl;
    }
}

// Now it's not just from cancels (counting), it needs to ensure there's at least one non-faulty.
/*
int getCommitCountFromCancels(vector<int> cancels, int f)
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
// 'selections' is only used for dubious commit 0 case (on primary 0)
int getCommitCountFromCancels(vector<int> cancels, vector<vector<pair<int, int>>> selections, int f)
{
    // compute commit counts
    // commit zero (X_0) may be issued when any zero is present
    // commit one (X_1) may be issued when 2*f+1 ones is present

    bool hasCommitOne = false;
    bool hasCommitZero = false; // a little bit more complex

    //bool hasBadZero = false;     // bad zero condition, may help commit one too... BadZero + 2 ones (N=4).. when 1 is faulty
    //bool hasTrustedZero = false; // trusted zero should be the same as BadZero above...

    // look for zero commits
    for (unsigned i = 0; i < cancels.size(); i++)
    {
        if (cancels[i] == 0)
        {
            hasCommitZero = true;
            break;
        }
        // if not cancel zero, proceed...
        //if (cancels[i] != 0)
        //    continue;

        // found POSSIBLE X_0

        /*
        // must verify primary edge case
        if (i == 0) // may be tricky (mandatory for replica 0 to issue cancel 0)
        {
            // if primary 0 has another independent confirmation, then it's good
            int confirmations = 0;
            // selections contains only prepare responses (not request!)
            for (unsigned k = 0; k < selections[0].size(); k++)
                if ((selections[0][k].second == 0)) //&& (selections[0][k].first != 0)) // a zero from someone else
                    confirmations++;                // including itself
            if (confirmations >= 1)                  // more than itself
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
        */
    } // end looking for zeros

    // look for one commits
    int ones = 0;
    for (unsigned i = 0; i < cancels.size(); i++)
        if (cancels[i] == 1)
            ones++;
    if (ones >= 2 * f + 1) // found a possible X_1
        hasCommitOne = true;

    // special case: two one's and one BadZero (N=4) and a failed node...
    //if (ones + hasBadZero == 2 * f + 1) // since ones are never in primary when BadZero is, this is good.
    //    hasCommitOne = true;

    //cout << "trustedzero: " << hasTrustedZero << endl;
    //cout << "badzero: " << hasBadZero << endl;
    cout << "commit0: " << hasCommitZero << endl;
    cout << "commit1: " << hasCommitOne << endl;

    return hasCommitZero + hasCommitOne;
}

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
    // I think I got it now...
    // Cancel 0 must enforce a commit on that level, otherwise it's wrong and must be banned
    // that is, if it triggers a bad block (invalid) it should be blacklisted, and if no other option is available, then change view
    // (or reset round -> probably best, since it may have "poisoned" things already, TODO think about)
    // so, on general, it can add unexisting tx on block, that will not be found, then change view is triggered.
    // change view still necessary for this case, and also for very async network (huge delays)

/*
goes back to:
R_0 | 0 |       1(2)    1(3)    | Cancel 0
R_1 | 1 |       1(2)    1(3)    | Cancel 1
R_2 | 1 |       1(2)    1(3)    | Cancel 1
R_3 | 1 |       1(3)    1(2)    | Cancel 1
commit0: 1
commit1: 1
possible commits: 2
*/

    cout << "======== begin tests ========" << endl;
    //srand(time(NULL));
    srand(0);

    int f = 1; // N = 4

    for (unsigned test = 0; test < 100; test++)
    {
        cout << endl;
        cout << "TEST: " << test << endl;

        int faulty = rand() % 2;

        //vector<int> vec = makeTest(f, 0); // 0 buggy
        //vector<int> vec = makeTest(f, 1); // 1 buggy
        vector<int> choices = makeTest(f, faulty);
        printv(choices);

        vector<int> choicesi0 = make2f(choices, f, 0); // responses for replica 0
        printv(choicesi0);

        vector<vector<pair<int, int>>> sel = selections(choices, f);

        vector<int> cancels = getCancels(choices, sel, f);

        printTable(choices, sel, cancels, f);

        //printv(cancels);

        int commits = getCommitCountFromCancels(cancels, sel, f);
        cout << "possible commits: " << commits << endl;
        if (commits != 1)
        {
            cout << "SPORK!"
                 << " Multiple or Zero commits" << endl;
            exit(1);
        }
    }

    cout << "finished successfully" << endl;
    cout << endl;
    return 0;
}
