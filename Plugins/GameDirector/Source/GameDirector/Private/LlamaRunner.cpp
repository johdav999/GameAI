#include "LlamaRunner.h"

#include "Misc/Paths.h"

#include <llama.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

DEFINE_LOG_CATEGORY_STATIC(LogLlamaRunner, Log, All);

FLlamaRunner::FLlamaRunner()
    : Model(nullptr)
    , Context(nullptr)
    , bIsLoaded(false)
{
}

FLlamaRunner::~FLlamaRunner()
{
    Release();
}

bool FLlamaRunner::LoadModel(const FString& ModelPath)
{
    Release();

    if (!FPaths::FileExists(ModelPath))
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Unable to locate llama model at path: %s"), *ModelPath);
        return false;
    }

    const std::string ModelPathUtf8 = TCHAR_TO_UTF8(*ModelPath);

    llama_model_params ModelParams = llama_model_default_params();
    Model = llama_load_model_from_file(ModelPathUtf8.c_str(), ModelParams);

    if (!Model)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Failed to load llama model from %s"), *ModelPath);
        return false;
    }

    llama_context_params ContextParams = llama_context_default_params();
    Context = llama_new_context_with_model(Model, ContextParams);

    if (!Context)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Failed to create llama context for %s"), *ModelPath);
        Release();
        return false;
    }

    LoadedModelPath = ModelPath;
    bIsLoaded = true;

    UE_LOG(LogLlamaRunner, Log, TEXT("Loaded llama model from %s"), *ModelPath);
    return true;
}

FString FLlamaRunner::RunInference(const FString& Prompt)
{
    if (!bIsLoaded || Context == nullptr || Model == nullptr)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("RunInference called before model was loaded."));
        return FString();
    }

    const llama_vocab* Vocab = llama_model_get_vocab(Model);
    if (!Vocab)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Failed to get vocab from model."));
        return FString();
    }

    // --- Compose full prompt to match training style ---
    const char* SystemJSON =
        "SYSTEM: You are GameDirector AI for an FPS. "
        "Only reply with a single JSON object with keys: schema, intent, reason, tool_calls. "
        "Do not write prose. Numbers must respect bounds. "
        "Levels are 1..5. Fine nudges are -0.10..+0.10.";

    std::string FullPrompt =
        std::string(SystemJSON) +
        "\nINPUT: " + TCHAR_TO_UTF8(*Prompt) +
        "\nOUTPUT: ";

    // --- Tokenize ---
    int32_t tok_needed = llama_tokenize(Vocab, FullPrompt.c_str(), (int32_t)FullPrompt.size(),
        nullptr, 0, true, true);
    if (tok_needed < 0) tok_needed = -tok_needed;
    if (tok_needed <= 0)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Tokenization failed for prompt."));
        return FString();
    }

    std::vector<llama_token> tokens((size_t)tok_needed);
    int32_t tok_count = llama_tokenize(Vocab, FullPrompt.c_str(), (int32_t)FullPrompt.size(),
        tokens.data(), (int32_t)tokens.size(), true, true);
    if (tok_count <= 0)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Tokenization write failed for prompt."));
        return FString();
    }

    // --- Decode prompt (last token only computes logits) ---
    llama_batch prompt_batch = llama_batch_init(tok_count, 0, 1);
    prompt_batch.n_tokens = tok_count;
    for (int i = 0; i < tok_count; ++i)
    {
        prompt_batch.token[i] = tokens[i];
        prompt_batch.pos[i] = i;
        prompt_batch.n_seq_id[i] = 1;
        prompt_batch.seq_id[i][0] = 0;
        prompt_batch.logits[i] = (i == tok_count - 1);
    }

    {
        FScopeLock Lock(&DecodeMutex);
        if (llama_decode(Context, prompt_batch) < 0)
        {
            UE_LOG(LogLlamaRunner, Error, TEXT("llama_decode(prompt) failed."));
            llama_batch_free(prompt_batch);
            return FString();
        }
    }

    // --- Sampling setup ---
    const int n_vocab = llama_vocab_n_tokens(Vocab);
    std::vector<float> work_logits((size_t)n_vocab);
    std::vector<int> idx((size_t)n_vocab);
    std::iota(idx.begin(), idx.end(), 0);

    std::mt19937 rng((uint32_t)(llama_time_us() & 0xFFFFFFFFu));
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);

    auto greedy_pick = [&](const float* l) -> int {
        int best = 0; float m = l[0];
        for (int i = 1; i < n_vocab; ++i) if (l[i] > m) { m = l[i]; best = i; }
        return best;
        };

    auto sample_topk_topp_temp = [&](const float* logits, int top_k, float top_p, float temp) -> int {
        std::memcpy(work_logits.data(), logits, sizeof(float) * (size_t)n_vocab);

        if (temp > 0.0f)
        {
            const float invT = 1.0f / temp;
            for (int i = 0; i < n_vocab; ++i) work_logits[i] *= invT;
        }

        int K = (top_k > 0) ? std::min(top_k, n_vocab) : n_vocab;
        std::nth_element(idx.begin(), idx.begin() + K, idx.end(),
            [&](int a, int b) { return work_logits[a] > work_logits[b]; });
        idx.resize((size_t)K);

        float maxl = -FLT_MAX;
        for (int id : idx) maxl = std::max(maxl, work_logits[id]);
        float sum = 0.0f;
        for (int id : idx) { work_logits[id] = std::exp(work_logits[id] - maxl); sum += work_logits[id]; }
        if (sum <= 0.0f) return idx[0];
        for (int id : idx) work_logits[id] /= sum;

        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return work_logits[a] > work_logits[b]; });

        if (top_p > 0.0f && top_p < 1.0f)
        {
            float cum = 0.0f; size_t cut = idx.size();
            for (size_t j = 0; j < idx.size(); ++j)
            {
                cum += work_logits[idx[j]];
                if (cum >= top_p) { cut = j + 1; break; }
            }
            if (cut < idx.size()) idx.resize(cut);
        }

        float r = uni(rng);
        float acc = 0.0f;
        int choice = idx.back();
        for (int id : idx) { acc += work_logits[id]; if (r <= acc) { choice = id; break; } }

        idx.resize((size_t)n_vocab);
        std::iota(idx.begin(), idx.end(), 0);
        return choice;
        };

    // --- JSON done detector ---
    auto json_done = [&](const std::string& s) -> bool {
        int depth = 0;
        bool in_q = false, escp = false;
        for (unsigned char ch : s)
        {
            if (escp) { escp = false; continue; }
            if (ch == '\\') { escp = true; continue; }
            if (ch == '"') { in_q = !in_q; continue; }
            if (in_q) continue;
            if (ch == '{') ++depth;
            else if (ch == '}')
            {
                if (depth > 0) --depth;
                if (depth == 0) return true;
            }
        }
        return false;
        };

    // --- Generation loop ---
    std::vector<llama_token> out_tokens;
    out_tokens.reserve(512);

    llama_batch step = llama_batch_init(1, 0, 1);
    int cur_pos = tok_count;
    std::string stream;
    stream.reserve(4096);

    int maxNew = 512;
    int top_k = 40;
    float top_p = 0.9f;
    float temp = 0.7f; // slightly lower for schema stability

    for (int i = 0; i < maxNew; ++i)
    {
        const float* logits = llama_get_logits_ith(Context, -1);
        if (!logits) break;

        int id = (temp <= 0.0f && top_k <= 1)
            ? greedy_pick(logits)
            : sample_topk_topp_temp(logits, top_k, top_p, temp);

        if (llama_vocab_is_eog(Vocab, (llama_token)id))
            break;

        char piece[256];
        int pn = llama_token_to_piece(Vocab, (llama_token)id, piece, sizeof(piece), 0, false);
        if (pn > 0) stream.append(piece, piece + pn);
        out_tokens.push_back((llama_token)id);

        if (json_done(stream))
        {
            UE_LOG(LogLlamaRunner, Display, TEXT("Detected JSON close at token %d"), i);
            break;
        }

        step.n_tokens = 1;
        step.token[0] = (llama_token)id;
        step.pos[0] = cur_pos++;
        step.n_seq_id[0] = 1;
        step.seq_id[0][0] = 0;
        step.logits[0] = 1;

        {
            FScopeLock Lock(&DecodeMutex);
            if (llama_decode(Context, step) < 0) break;
        }
    }

    llama_batch_free(step);
    llama_batch_free(prompt_batch);

    // --- Use direct stream output ---
    std::string out_str = stream;
    if (out_str.empty() && !out_tokens.empty())
    {
        out_str.assign(out_tokens.size() * 8, '\0');
        int32_t w = llama_detokenize(Vocab, out_tokens.data(), (int32_t)out_tokens.size(),
            out_str.data(), (int32_t)out_str.size(),
            true, false);
        if (w > 0) out_str.resize((size_t)w); else out_str.clear();
    }

    FString Output(UTF8_TO_TCHAR(out_str.c_str()));
    UE_LOG(LogLlamaRunner, Display, TEXT("Inference completed. Output: %s"), *Output);
    return Output;
}



void FLlamaRunner::Release()
{
    if (Context)
    {
        llama_free(Context);
        Context = nullptr;
    }

    if (Model)
    {
        llama_free_model(Model);
        Model = nullptr;
    }

    if (!LoadedModelPath.IsEmpty())
    {
        UE_LOG(LogLlamaRunner, Log, TEXT("Unloaded llama model %s"), *LoadedModelPath);
    }

    LoadedModelPath.Reset();
    bIsLoaded = false;
}
