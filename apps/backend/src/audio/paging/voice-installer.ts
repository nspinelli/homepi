import { access, mkdir, writeFile } from "node:fs/promises";
import { constants as fsConstants } from "node:fs";

import type { Logger } from "@homepi/core-logging";

import type { PagingClient } from "./paging-client.js";
import {
  findCatalogVoice,
  PAGING_VOICE_STORAGE_DIR,
  resolvePagingVoiceFilePaths,
  type PagingVoiceCatalogEntry,
} from "./voice-catalog.js";

/**
 * Options for installing a catalog voice on the Pi.
 */
export interface InstallCatalogVoiceOptions {
  /** Paging Unix socket client. */
  client: PagingClient;
  /** Voice identifier to install. */
  voiceId: string;
  /** Whether to set the voice as default after install. */
  setDefault?: boolean;
  /** Request correlation id. */
  correlationId: string;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Confirms that a file exists on disk.
 * @param filePath - Absolute file path.
 * @returns True when the file is present.
 */
async function fileExists(filePath: string): Promise<boolean> {
  try {
    await access(filePath, fsConstants.F_OK);
    return true;
  } catch {
    return false;
  }
}

/**
 * Downloads a remote Piper artifact to disk.
 * @param url - Remote file URL.
 * @param destinationPath - Local destination path.
 */
async function downloadFile(url: string, destinationPath: string): Promise<void> {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to download ${url}: HTTP ${response.status}`);
  }

  const bytes = Buffer.from(await response.arrayBuffer());
  if (bytes.length === 0) {
    throw new Error(`Downloaded file was empty: ${url}`);
  }

  await writeFile(destinationPath, bytes);
}

/**
 * Downloads ONNX model and JSON config for a catalog voice when needed.
 * @param voice - Catalog voice metadata.
 */
async function ensureVoiceFiles(voice: PagingVoiceCatalogEntry): Promise<{
  modelPath: string;
  configPath: string;
}> {
  const paths = resolvePagingVoiceFilePaths(voice.voiceId);
  await mkdir(PAGING_VOICE_STORAGE_DIR, { recursive: true });

  const modelExists = await fileExists(paths.modelPath);
  const configExists = await fileExists(paths.configPath);

  if (!modelExists) {
    await downloadFile(paths.modelUrl, paths.modelPath);
  }
  if (!configExists) {
    await downloadFile(paths.configUrl, paths.configPath);
  }

  return {
    modelPath: paths.modelPath,
    configPath: paths.configPath,
  };
}

/**
 * Installs a catalog voice by downloading Piper artifacts and registering them in paging service state.
 * @param options - Install options.
 * @returns Install result payload.
 */
export async function installCatalogVoice(
  options: InstallCatalogVoiceOptions
): Promise<Record<string, unknown>> {
  const { client, voiceId, setDefault = false, correlationId, logger } = options;
  const catalogVoice = await findCatalogVoice(voiceId);
  if (!catalogVoice) {
    throw new Error(`Voice not found in catalog: ${voiceId}`);
  }

  const [config, installedVoices] = await Promise.all([
    client.getConfig(correlationId),
    client.getVoices(correlationId),
  ]);

  const alreadyInstalled = installedVoices.voices.some(
    (voice) => voice.voiceId === voiceId && voice.installed
  );

  if (!alreadyInstalled) {
    const installedCount = installedVoices.voices.filter((voice) => voice.installed).length;
    if (installedCount >= config.maxInstalledVoices) {
      const error = new Error("Maximum installed voices reached. Remove one before downloading another.");
      (error as Error & { code?: string }).code = "max_installed_voices_reached";
      throw error;
    }

    const files = await ensureVoiceFiles(catalogVoice);
    logger.info({
      module: "app.backend.paging",
      event: "voice_download_complete",
      correlationId,
      message: "Downloaded Piper voice artifacts",
      data: { voiceId, modelPath: files.modelPath },
    });

    await client.installVoice(
      {
        voiceId,
        displayName: catalogVoice.displayName,
        languageCode: catalogVoice.languageCode,
        quality: catalogVoice.quality,
        modelPath: files.modelPath,
        configPath: files.configPath,
        isBundled: catalogVoice.isBundled,
      },
      correlationId
    );
  }

  if (setDefault) {
    await client.updateConfig({ defaultVoiceId: voiceId, activeVoiceId: voiceId }, correlationId);
  }

  await client.reloadVoice(voiceId, correlationId);

  return {
    voiceId,
    installed: true,
    isDefault: setDefault || config.defaultVoiceId === voiceId,
    workerReloading: true,
  };
}
