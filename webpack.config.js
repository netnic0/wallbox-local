const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CompressionPlugin = require('compression-webpack-plugin');
const HTMLInlineCSSPlugin = require('html-inline-css-webpack-plugin').default;
const HtmlInlineScriptPlugin = require('html-inline-script-webpack-plugin');
const CopyPlugin = require('copy-webpack-plugin');
const { CleanWebpackPlugin } = require('clean-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const CssMinimizerPlugin = require('css-minimizer-webpack-plugin');

module.exports = (env, argv) => {
  const isProd = argv && argv.mode === 'production';

  const plugins = [
    new CleanWebpackPlugin(),
    new CopyPlugin({
      patterns: [{ from: 'www/assets/favicon.png', to: '' }],
    }),
    new MiniCssExtractPlugin({
      filename: '[name].css',
      chunkFilename: '[id].css',
    }),
    new HtmlWebpackPlugin({
      template: 'www/index.html',
      inject: 'body',
    }),
  ];

  if (isProd) {
    plugins.push(new HTMLInlineCSSPlugin());
    plugins.push(new HtmlInlineScriptPlugin([/.+[.]js$/u]));
    plugins.push(new CompressionPlugin({ deleteOriginalAssets: true }));
  }

  return {
    entry: './www/app/app.js',
    output: {
      path: path.resolve(__dirname, 'dist'),
      filename: 'bundle.min.js',
      clean: true,
    },
    module: {
      rules: [
        {
          test: /\.js$/u,
          exclude: /node_modules/u,
          use: { loader: 'babel-loader' },
        },
        {
          test: /\.css$/u,
          use: [MiniCssExtractPlugin.loader, 'css-loader'],
        },
      ],
    },
    optimization: {
      minimize: isProd,
      minimizer: isProd ? ['...', new CssMinimizerPlugin()] : [],
    },
    performance: isProd
      ? {
          maxAssetSize: 150 * 1024,
          maxEntrypointSize: 150 * 1024,
          hints: 'error',
        }
      : { hints: false },
    plugins,
    devServer: {
      port: 3000,
      static: {
        directory: path.resolve(__dirname, 'dist'),
        serveIndex: true,
      },
    },
  };
};